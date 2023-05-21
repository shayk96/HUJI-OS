#include <sched.h>
#include <cstdlib>
#include <sys/wait.h>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <fstream>

#define STACK 8192
#define MODE 0755
#define SUCCESS 0
#define ROOT_BASE "/"
#define CGROUP_PROCS_PATH "/sys/fs/cgroup/pids/cgroup.procs"
#define PIDSMAXPATH "/sys/fs/cgroup/pids/pids.max"
#define NOTIFY_PATH "/sys/fs/cgroup/pids/notify_on_release"
#define PIDS_PATH "/sys/fs/cgroup/pids"
#define CGROUP_PATH "/sys/fs/cgroup"
#define SYS_PATH "/sys/fs"
#define ERROR_HOST_NAME "system error: host name is wrong"
#define ERROR_ROOT_PATH "system error: root path is wrong"
#define ERROR_DIRECTORIES "system error: failed to create directories"
#define ERROR_ACTIVE_DIR_TO_ROOT "system error: failed to change working directory to root"
#define ERROR_COMMAND "system error: failed to run the command"
#define ERROR_MEM_ALLOC "system error: failed to allocate memory to stack"
#define ERROR_CLONE "system error: failed to clone to process"
#define DIR_REMOVE_ERROR "system error: failed to delete directories and files"
#define ERROR_MOUNT "system error: failed to mount file system"
#define ERROR_UMOUNT "system error: failed to unmount file system"
#define DELETE_ON_EXIT 1
#define HOST_NAME 1
#define ROOT 2
#define MAX_PROCESSES_INDEX 3
#define COMMAND 4
#define ARGUMENTS 5
#define LAST_ARGUMENT 0
#define FAILURE -1
#define SPECIAL_FILE "proc"
#define SLASH_SPECIAL_FILE "/proc"

using namespace std;
int *stack;

int mkdirs() {
  if ((mkdir(SYS_PATH,MODE) != SUCCESS)){
    return EXIT_FAILURE;
  }
  if ((mkdir(CGROUP_PATH,MODE) != SUCCESS)){
    return EXIT_FAILURE;
  }
  if ((mkdir(PIDS_PATH,MODE) != SUCCESS)){
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int rmdirs(char* path) {
  char* original = (char*) malloc(strlen(path));
  if (original == nullptr) {
    return EXIT_FAILURE;
  }
  strcpy(original,path);
  if (remove(strcat(path, CGROUP_PROCS_PATH)) != SUCCESS){
    free(original);
    return EXIT_FAILURE;
  }
  strcpy(path,original);
  if (remove(strcat(path, PIDSMAXPATH)) != SUCCESS){
    free(original);
    return EXIT_FAILURE;
  }
  strcpy(path,original);
  if (remove(strcat(path, NOTIFY_PATH)) != SUCCESS){
    free(original);
    return EXIT_FAILURE;
  }
  strcpy(path,original);
  if ((rmdir(strcat(path,PIDS_PATH)) != SUCCESS)){
    free(original);
    return EXIT_FAILURE;
  }
  strcpy(path,original);
  if ((rmdir(strcat(path,CGROUP_PATH)) != SUCCESS)){
    free(original);
    return EXIT_FAILURE;
  }
  strcpy(path,original);
  if ((rmdir(strcat(path,SYS_PATH )) != SUCCESS)){
    free(original);
    return EXIT_FAILURE;
  }
  strcpy(path,original);
  free(original);
  return EXIT_SUCCESS;
}

void create_files(char *max_p){
  ofstream procs;
  procs.open(CGROUP_PROCS_PATH);
  procs << getpid() << endl;
  ofstream max;
  max.open(PIDSMAXPATH);
  max << max_p << endl;
  ofstream notify;
  notify.open(NOTIFY_PATH);
  notify << DELETE_ON_EXIT << endl;
  procs.close();
  max.close();
  notify.close();
}

int child(void* argv) {
  char** args = (char**) argv;
  if (sethostname(args[HOST_NAME], strlen(args[HOST_NAME])) != SUCCESS){
    cerr << ERROR_HOST_NAME << endl;
    delete[] stack;
    exit(EXIT_FAILURE);
  }
  if (chroot(args[ROOT]) != SUCCESS){
    cerr << ERROR_ROOT_PATH << endl;
    delete[] stack;
    exit(EXIT_FAILURE);
  }
  if (mkdirs() == EXIT_FAILURE) {
    cerr << ERROR_DIRECTORIES << endl;
    delete[] stack;
    exit(EXIT_FAILURE);
  }
  create_files(args[MAX_PROCESSES_INDEX]);
  if (chdir(ROOT_BASE) != SUCCESS){
    cerr << ERROR_ACTIVE_DIR_TO_ROOT << endl;
    delete[] stack;
    exit(EXIT_FAILURE);
  }
  if (mount(SPECIAL_FILE, SLASH_SPECIAL_FILE, SPECIAL_FILE, 0, 0) != SUCCESS) {
    cerr << ERROR_MOUNT << endl;
    delete[] stack;
    exit(EXIT_FAILURE);
  }
  char* arguments[] = {args[COMMAND],args[ARGUMENTS], (char*) LAST_ARGUMENT};
  if (execvp(args[COMMAND], arguments) == FAILURE) {
    cerr << ERROR_COMMAND << endl;
    delete[] stack;
    exit(EXIT_FAILURE);
  }
  return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
  stack = new(nothrow) int[STACK];
  if (stack == nullptr) {
    cerr << ERROR_MEM_ALLOC << endl;
    exit(EXIT_FAILURE);
  }
  int child_pid = clone(child, stack + STACK, CLONE_NEWPID | SIGCHLD | CLONE_NEWUTS | CLONE_NEWNS, argv);
  if (child_pid == FAILURE) {
    delete[] stack;
    cerr << ERROR_CLONE << endl;
    exit(EXIT_FAILURE);
  }
  wait(nullptr);
  delete[] stack;
  if (rmdirs(argv[ROOT]) != SUCCESS) {
    cerr << DIR_REMOVE_ERROR << endl;
    exit(EXIT_FAILURE);
  }
  char* root_path = argv[ROOT];
  if (umount(strcat(root_path, SLASH_SPECIAL_FILE)) != SUCCESS) {
    cerr << ERROR_UMOUNT << endl;
    exit(EXIT_FAILURE);
  }
  chdir(argv[ROOT]);
}
