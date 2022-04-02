#include "NanoLog.hpp"
#include <bits/stdc++.h>
#include <unistd.h>
#include <linux/types.h>

#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <fstream>

__u64 rdtsc()
{
    __u32 lo, hi;
    __asm__ __volatile__("rdtsc"
                         : "=a"(lo), "=d"(hi));
    return (__u64)hi << 32 | lo;
}

bool operator<(const timespec &lhs, const timespec &rhs)
{
    if (lhs.tv_sec == rhs.tv_sec)
        return lhs.tv_nsec < rhs.tv_nsec;
    else
        return lhs.tv_sec < rhs.tv_sec;
}
timespec operator+(const timespec &lhs, const timespec &rhs)
{
    timespec sum = {lhs.tv_sec + rhs.tv_sec, lhs.tv_nsec + rhs.tv_nsec};
    if (sum.tv_nsec > 1e9)
    {
        sum.tv_nsec -= 1e9;
        sum.tv_sec += 1;
    }
    return sum;
}
timespec operator-(const timespec &lhs, const timespec &rhs)
{
    timespec delta = {lhs.tv_sec - rhs.tv_sec, lhs.tv_nsec - rhs.tv_nsec};
    if (delta.tv_nsec < 0)
    {
        delta.tv_sec -= 1;
        delta.tv_nsec += 1e9;
    }
    return delta;
}
timespec operator/(const timespec &lhs, const int &rhs)
{
    
        double timeSum = (lhs.tv_sec + 1e-9 * lhs.tv_nsec) / rhs;
        return {long(timeSum), long((timeSum - long(timeSum)) * 1e9)};
}

class PerfTool
{
private:
    timespec beginTime = {0, 0}, endTime = {0, 0};
    // Paramaters initialize
    const std::string describe;
    const bool bUseCPUClock;
    const int reportTimes, subReportTimes;
    int windowSize;

    // control output
    long timeScale;
    std::string timeMessage;

    // metrics initialize
    std::vector<timespec> partOfTimeList;
    std::list<std::vector<timespec>> rollingTimeList;
    int reportTimesCounter = 0, subReportTimesCounter = 0;
    timespec sum, maxDeltaTime, minDeltaTime;

    void logDescribeInfo(void)
    {
        time_t tm;
        time(&tm);
        char tmp[64];
        strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&tm));
        LOG_INFO << '<' << std::string(tmp) << "> " << describe << " statistics";
    }

    void logMetricInfo(const std::string metricName, const timespec &metricData)
    {
        LOG_INFO << metricName << ":" << metricData.tv_sec << "s" << metricData.tv_nsec / timeScale << timeMessage;
    }

    void logOnlineInfo(void)
    {
        logMetricInfo("Max", maxDeltaTime);
        logMetricInfo("Min", minDeltaTime);
        logMetricInfo("Mean", sum / subReportTimes);
    }

    void logInfo(void)
    {
        logDescribeInfo();

        std::vector<timespec> allOfTimeList;
        for (std::vector<timespec> &partTL : rollingTimeList)
        {
            allOfTimeList.insert(allOfTimeList.end(), partTL.begin(), partTL.end());
        }
        std::sort(allOfTimeList.begin(), allOfTimeList.end());
        // caculate mean
        timespec timeSum = {0, 0};
        for (timespec& time : allOfTimeList)
        {
            timeSum = timeSum + time;
        }

        logMetricInfo("Max", allOfTimeList[allOfTimeList.size() - 1]);
        logMetricInfo("Min", allOfTimeList[0]);
        logMetricInfo("Mean", timeSum / allOfTimeList.size());
        logMetricInfo("95%", allOfTimeList[long(allOfTimeList.size() * 0.95)]);
        logMetricInfo("75%", allOfTimeList[long(allOfTimeList.size() * 0.75)]);
        logMetricInfo("50%", allOfTimeList[long(allOfTimeList.size() * 0.50)]);
        logMetricInfo("25%", allOfTimeList[long(allOfTimeList.size() * 0.25)]);
    }

    void initOnlineMetrics(void)
    {
        sum = {0, 0}, maxDeltaTime = {0, 0}, minDeltaTime = {LONG_MAX, 0};
    }

    void getTimeByFunction(timespec &selfTime, u_int64_t time = 0)
    {
        if (time == 0)
        {
            clock_gettime(CLOCK_REALTIME, &selfTime);
        }
        else
        {
            selfTime = {long(time), 0};
        }
    }

public:
    PerfTool(const char *describe,
             int reportTimes,
             int subReportTimes,
             int rollingWindowSize = 0,
             bool rolling = false,
             int unit = 0,
             bool bUseCPUClock = false)
        : reportTimes(reportTimes),
          subReportTimes(subReportTimes),
          bUseCPUClock(bUseCPUClock),
          describe(describe)
    {
        nanolog::initialize(nanolog::GuaranteedLogger(), get_current_dir_name(), "nanolog", 1);
        char timeMessageList[4][3] = {"ns", "us", "ms", "s"};
        timeScale = pow(1000, unit);
        timeMessage = timeMessageList[unit];
        windowSize = rolling ? (((rollingWindowSize - 1) / reportTimes + 1) * reportTimes) : reportTimes;
        partOfTimeList.resize(reportTimes);
        initOnlineMetrics();
    };

    PerfTool(const char *describe,
             PerfTool *master)
        : reportTimes(master->reportTimes),
          subReportTimes(master->subReportTimes),
          timeScale(master->timeScale),
          timeMessage(master->timeMessage),
          bUseCPUClock(master->bUseCPUClock),
          windowSize(master->windowSize),
          describe(describe)
    {
        partOfTimeList.assign((master->partOfTimeList).begin(), (master->partOfTimeList).end());
        nanolog::initialize(nanolog::GuaranteedLogger(), get_current_dir_name(), "nanolog", 1);
        partOfTimeList.resize(reportTimes);
        initOnlineMetrics();
    };

    ~PerfTool()
    {
        std::vector<timespec>().swap(partOfTimeList);
    };

    void begin(uint64_t time = 0)
    {
        if (bUseCPUClock == false)
        {
            getTimeByFunction(beginTime, time);
        }
        else
        {
            beginTime = {(long)rdtsc(), 0};
        }
    }
    void end(uint64_t time = 0)
    {
        if (bUseCPUClock == false)
        {
            getTimeByFunction(endTime, time);
        }
        else
        {
            endTime = {(long)rdtsc(), 0};
        }
    };

    void report(bool bForce = false)
    {
        const struct timespec deltaTime = endTime - beginTime;
        partOfTimeList[reportTimesCounter] = deltaTime;

        sum = sum + deltaTime;
        maxDeltaTime = maxDeltaTime < deltaTime ? deltaTime : maxDeltaTime;
        minDeltaTime = deltaTime < minDeltaTime ? deltaTime : minDeltaTime;

        if (++reportTimesCounter == reportTimes)
        {
            reportTimesCounter = 0;
        }
        if (++subReportTimesCounter == subReportTimes)
        {
            subReportTimesCounter = 0;
        }

        if (bForce == true || subReportTimesCounter == 0)
        {
            logOnlineInfo();
            initOnlineMetrics();
        }
        if (bForce == true || reportTimesCounter == 0)
        {
            rollingTimeList.push_back(partOfTimeList);
            if (rollingTimeList.size() > windowSize)
            {
                rollingTimeList.pop_front();
            }
            logInfo();
        }
    };

    void analysisReport(bool bForce = false)
    {
        std::ofstream file("perfTool.json");
        cereal::JSONOutputArchive archive(file);

        std::vector<timespec> allOfTimeList;
        for (std::vector<timespec> &partTL : rollingTimeList)
        {
            allOfTimeList.insert(allOfTimeList.end(), partTL.begin(), partTL.end());
        }
        std::sort(allOfTimeList.begin(), allOfTimeList.end());

        archive(cereal::make_nvp("Max", allOfTimeList[allOfTimeList.size() - 1].tv_sec),
                cereal::make_nvp("Min", allOfTimeList[0].tv_sec),
                cereal::make_nvp("95%", allOfTimeList[long(allOfTimeList.size() * 0.95)].tv_sec),
                cereal::make_nvp("75%", allOfTimeList[long(allOfTimeList.size() * 0.75)].tv_sec),
                cereal::make_nvp("50%", allOfTimeList[long(allOfTimeList.size() * 0.50)].tv_sec),
                cereal::make_nvp("25%", allOfTimeList[long(allOfTimeList.size() * 0.25)].tv_sec));
    };
};

int main(void)
{
    PerfTool ttt = PerfTool("test",2, 1);
    for (int i = 0; i < 4; ++i)
    {
        ttt.begin();
        sleep(1 + i);
        ttt.end();
        ttt.report();
    }
    ttt.analysisReport(true);
}

// rolling analysisReport尚未完成