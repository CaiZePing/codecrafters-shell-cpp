#include "command.hpp"

#include <signal.h>
#include <sys/wait.h>
#include <format>

namespace cmd {
const char* StateStr[3] = {"Running", "Stopped", "Done"};
// 忽略信号
static void ignore_signal(int sig) {
    struct sigaction sa = {};
    sa.sa_handler = SIG_IGN;
    sigaction(sig, &sa, nullptr);
}

// 恢复信号默认行为
static void restore_signal(int sig) {
    struct sigaction sa = {};
    sa.sa_handler = SIG_DFL;
    sigaction(sig, &sa, nullptr);
}

bool Jobs::addJob(Job job) {
    if (job_list.size() >= MAX_JOBS) {
        Jobs::instance().removeDoneJobs();
        if (job_list.size() >= MAX_JOBS) {
            return false;
        }
    }
    job_list.push_back(std::move(job));
    return true;
}
Job& Jobs::getJobById(size_t job_id) {
    for (auto &job : job_list) {
        if (job.getJobId() == job_id) {
            return job;
        }
    }
    throw std::invalid_argument("Job not found");
}
Job& Jobs::getJobByPgid(pid_t pgid) {
    for (auto &job : job_list) {
        if (job.getPgid() == pgid) {
            return job;
        }
    }
    throw std::invalid_argument("Job not found");
}
void Jobs::showJobs() {
    Jobs::instance().checkAndUpdateJobState();
    for (int i = 0; i < job_list.size(); i++) {
        const auto& job = job_list[i];
        if (i == job_list.size() - 1) {
            std::cout << std::format("[{}]+  {:27}{}", job.getJobId(), StateStr[static_cast<int>(job.getState())], job.getCommand()) << std::endl;
        } else if (i == job_list.size() - 2) {
            std::cout << std::format("[{}]-  {:27}{}", job.getJobId(), StateStr[static_cast<int>(job.getState())], job.getCommand()) << std::endl;
        } else {
            std::cout << std::format("[{}]   {:27}{}", job.getJobId(), StateStr[static_cast<int>(job.getState())], job.getCommand()) << std::endl;
        }
    }
    Jobs::instance().removeDoneJobs();
}

void Jobs::removeDoneJobs() {
    job_list.erase(std::remove_if(job_list.begin(), job_list.end(), [](const Job& job) {
        return job.getState() == State::DONE;
    }), job_list.end());
}

void Jobs::checkAndUpdateJobState() {
    for (auto &job : job_list) {
        // 跳过已经结束的进程组
        if (job.getState() == State::DONE || job.getState() == State::STOPPED) {
            continue;
        }
        int state;
        pid_t result = waitpid(-job.getPgid(), &state, WNOHANG);
        if (result > 0) {
            if (WIFEXITED(state)) {
                job.setState(State::DONE);
            } else if (WIFSTOPPED(state)) {
                job.setState(State::STOPPED);
            }
        }
    }
}

void jobs(const std::vector<std::string>& parsed) {
    // Implementation for displaying jobs
    Jobs& jobs = Jobs::instance();
    jobs.showJobs();
}

// 执行外部命令，创建子进程，父进程等待子进程结束
void myexecv(const std::vector<std::string>& parsed, std::string command) {
  std::vector<const char*> argv;
  for (const auto& arg : parsed) {
    argv.push_back(arg.c_str());
  }
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid == 0) {
    // 子进程创建新组（暂时注释掉）
    // setpgid(0, 0);
    restore_signal(SIGINT);
    restore_signal(SIGTSTP);
    std::string cmd_path = findCmdInPath(argv[0]);
    if (!cmd_path.empty()) {
      execvp(cmd_path.c_str(), (char**)argv.data());
    } else {
      execvp(argv[0], (char**)argv.data());
    }
    // 如果execvp失败，子进程退出
    std::cerr << parsed[0] << ": command not found" << std::endl;
    exit(1);
  } else if (pid > 0) {
    // setpgid(pid, pid);  // 父进程：确保子进程在新进程组中（暂时注释掉）
    // 临时忽略 SIGTTOU 和 SIGTTIN 信号，避免 tcsetpgrp 调用被中断
    // struct sigaction old_sigttou, old_sigttin;
    ignore_signal(SIGINT);
    ignore_signal(SIGTSTP);
    // ignore_signal(SIGTTOU);
    // ignore_signal(SIGTTIN);
    
    // 将终端控制权交给子进程（暂时注释掉）
    // while (tcsetpgrp(STDIN_FILENO, pid) < 0) {
    //     if (errno != EINTR) {
    //         break;
    //     }
    // }
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            break;
        }
    }
    // 恢复终端控制权（暂时注释掉）
    // while (tcsetpgrp(STDIN_FILENO, getpgrp()) < 0) {
    //     if (errno != EINTR) {
    //         break;
    //     }
    // }
    
    // 恢复 SIGTTOU 和 SIGTTIN 信号（使用默认处理）（暂时注释掉）
    // restore_signal(SIGTTOU);
    // restore_signal(SIGTTIN);
    restore_signal(SIGINT);
    restore_signal(SIGTSTP);
    // Ctrl+Z 暂停
    if (WIFSTOPPED(status)) {
        // 此时才创建 Job 并加入列表
        Job new_job(pid, command);
        new_job.addProcess(pid);
        new_job.setState(State::STOPPED);
        Jobs::instance().addJob(new_job);
         std::cout << "[" << new_job.getJobId() << "] " << new_job.getPgid() << std::endl;
        // std::cout << std::format("[{}]+  {:27}{}", new_job.getJobId(), "Running", new_job.getCommand()) << std::endl;
    }
  } else {
    std::cerr << "fork failed" << std::endl;
  }
}

// 执行管道命令并捕获标准输出
std::string pipeexecv(const std::vector<std::string>& parsed, std::string command) {
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    std::cerr << "pipe failed" << std::endl;
    return "";
  }

  pid_t pid = fork();
  if (pid == 0) {
    // 子进程创建新组
    setpgid(0, 0);
    // 子进程：执行命令并将输出写入管道
    close(pipefd[0]); // 关闭读端
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO); // 也捕获标准错误
    close(pipefd[1]);
    
    std::vector<const char*> argv;
    for (const auto& arg : parsed) {
      argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);
    
    std::string cmd_path = findCmdInPath(argv[0]);
    if (!cmd_path.empty()) {
      execvp(cmd_path.c_str(), (char**)argv.data());
    } else {
      execvp(argv[0], (char**)argv.data());
    }
    exit(1);
  } else if (pid > 0) {
    // 父进程：读取管道输出
    close(pipefd[1]); // 关闭写端
    
    std::string output;
    char buffer[4096];
    ssize_t n;
    
    while ((n = read(pipefd[0], buffer, sizeof(buffer)-1)) > 0) {
      buffer[n] = '\0';
      output += buffer;
    }
    
    close(pipefd[0]);
    
    int status;
    waitpid(pid, &status, 0);
    
    return output;
  } else {
    std::cerr << "fork failed" << std::endl;
    close(pipefd[0]);
    close(pipefd[1]);
    return "";
  }
}

void bgexecv(const std::vector<std::string>& parsed, std::string command) {
    std::vector<const char*> argv;
  for (const auto& arg : parsed) {
    argv.push_back(arg.c_str());
  }
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid == 0) {
    setpgid(0, 0);
    std::string cmd_path = findCmdInPath(argv[0]);
    if (!cmd_path.empty()) {
      execvp(cmd_path.c_str(), (char**)argv.data());
    } else {
      execvp(argv[0], (char**)argv.data());
    }
    // 如果execvp失败，子进程退出
    std::cerr << parsed[0] << ": command not found" << std::endl;
    exit(1);
  } else if (pid > 0) {
    Job new_job(pid, command);
    new_job.addProcess(pid);
    Jobs::instance().addJob(new_job);
    std::cout << "[" << new_job.getJobId() << "] " << new_job.getPgid() << std::endl;
  } else {
    std::cerr << "fork failed" << std::endl;
  }
}

}