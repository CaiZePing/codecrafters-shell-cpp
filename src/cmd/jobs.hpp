#pragma once

#include "command.hpp"

namespace cmd {
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
    Job(size_t job_id, pid_t pgid, const std::string& cmd) 
        : job_id(job_id), pgid(pgid), command(cmd), state(State::RUNNING) {}

    // ---------------- 对外接口 ----------------
    void addProcess(pid_t pid) { processes.emplace_back(pid); }
    size_t getJobId() const { return job_id; }
    pid_t getPgid() const { return pgid; }
    State getState() const { return state; }
    const std::string& getCommand() const { return command; }
    const std::vector<Process>& getProcesses() const { return processes; }
    void setState(State new_state) { state = new_state; }
    void clearCommandSymbol() { if (!command.empty() && command.back() == '&') {command.pop_back(); command.pop_back();} }
    void checkAndUpdateState();
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
    Job& CreateJob(pid_t pgid, const std::string& cmd) {
        if (job_list.size() >= MAX_JOBS) {
            removeDoneJobs();
            if (job_list.size() >= MAX_JOBS) {
                throw std::runtime_error("Too many jobs");
            }
        }
        job_list.emplace_back(allocateJobId(), pgid, cmd);
        return job_list.back();
    }
    Job& getJobById(size_t job_id);
    Job& getJobByPgid(pid_t pgid);
    const std::vector<Job>& getJobs() const { return this->job_list; }
    void showJobs();
    void removeDoneJobs();
    void checkAndUpdateJobState();
    void checkAndRemoveJob();
    bool isFirst(const Job& job);
    bool isSecond(const Job& job);
private:
    // 私有构造函数（单例核心）
    Jobs() = default;
    Jobs(const Jobs&) = delete;
    Jobs& operator=(const Jobs&) = delete;

    const uint32_t MAX_JOBS = 1024;  // 最大作业数
    std::vector<Job> job_list;

    size_t allocateJobId();
};

// 当前的前台作业
static Job* current_foreground_job = nullptr;

void jobs(const std::vector<std::string>& parsed);

// 执行命令
void myexecv(const std::vector<std::string>& parsed, std::string command);
// 执行管道命令并捕获输出
std::string stdoutexecv(const std::vector<std::string>& parsed, std::string command);
// 在后台执行命令
void bgexecv(const std::vector<std::string>& parsed, std::string command);
// 执行管道命令
void pipeexecv(const std::vector<std::string>& parsed, std::string command);
// 在后台执行管道命令
void pipebgexecv(const std::vector<std::string>& parsed, std::string command);
// 执行内建命令
void builtinexecv(const std::string& input);
}