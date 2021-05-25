#ifndef EXECWRITER_H
#define EXECWRITER_H
#include <ostream>
#include <utility>
#include <fstream>
#include <thread>

#include "Domain.h"
#include "OrderBook.h"


class ExecWriter {

private:
    OrderBook::Ptr _orderBookPtr;
    std::ofstream _fileHandle;
    std::string _fileLocation;
    size_t _batchSize;
    std::vector<ExecReport::Ptr> _batch;
    std::thread _writerThread;
private:
    void main();
    void write(const ExecReport::Ptr& message);
    void writeBatch();
    static std::string formatTime(timestamp_t time_);
public:
    explicit ExecWriter(OrderBook::Ptr  orderBook_);
    ~ExecWriter();
    void start();
};

#endif //EXECWRITER_H
