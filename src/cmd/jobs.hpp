#pragma once

#include "command.hpp"

namespace cmd {
static size_t next_job_id = 1;  // 下一个作业ID
// 作业状态（严格对应终端进程状态）
enum class State : int8_t {
    RUNNING,  // 运行中（前台/后台）
    STOPPED,  // 暂停（Ctrl+Z）
    DONE      // 执行完毕
};
extern const char* StateStr[3];
// 进程
class Process {
public:
    Process(pid_t pid) : pid(pid), state(State::RUNNING) {}
    pid_t getPid() const { return pid; }
    State getState() const { return state; }
    void setState(State new_state) { state = new_state; }
private:
    pid_t pid;          // 进程PID（唯一）
    State state;          // 进程状态
};

// 工作类
class Job {
public:
    // 构造函数：初始化进程组ID + 作业命令
    Job(pid_t pgid, const std::string& cmd) 
        : job_id(next_job_id++), pgid(pgid), command(cmd), state(State::RUNNING) {}

    // ---------------- 对外接口 ----------------
    void addProcess(pid_t pid) { processes.emplace_back(pid); }
    size_t getJobId() const { return job_id; }
    pid_t getPgid() const { return pgid; }
    State getState() const { return state; }
    const std::string& getCommand() const { return command; }
    void clearCommandSymbol() { if (!command.empty() && command.back() == '&') command.pop_back(); command.pop_back(); }
    void setState(State new_state) { state = new_state; }
private:
    size_t job_id;            // job 的 id
    pid_t pgid;               // 组ID
    State state;              // 作业状态
    std::string command;      // 作业命令
    std::vector<Process> processes;  // 作业中的进程列表
};

class Jobs {
public:
    static Jobs& instance() {static Jobs jobs; return jobs;}
    bool addJob(Job job);
    Job& getJobById(size_t job_id);
    Job& getJobByPgid(pid_t pgid);
    void showJobs();
    void removeDoneJobs();
    void checkAndUpdateJobState();
private:
    // 私有构造函数（单例核心）
    Jobs() = default;
    Jobs(const Jobs&) = delete;
    Jobs& operator=(const Jobs&) = delete;

    const uint32_t MAX_JOBS = 1024;  // 最大作业数
    std::vector<Job> job_list;
};

// 当前的前台作业
static Job* current_foreground_job = nullptr;

void jobs(const std::vector<std::string>& parsed);

// 执行命令
void myexecv(const std::vector<std::string>& parsed, std::string command);
// 执行管道命令并捕获输出
std::string pipeexecv(const std::vector<std::string>& parsed, std::string command);
// 在后台执行命令
void bgexecv(const std::vector<std::string>& parsed, std::string command);
}