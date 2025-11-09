#include "ExecWriter.h"
#include <sstream>

ExecWriter::ExecWriter(OrderBook::Ptr orderBook_)
    : _orderBookPtr(std::move(orderBook_)), _fileHandle(),
      _fileLocation("./" + _orderBookPtr->symbol() + "_exec_report.log"),
      _batchSize(5), _batch() {}

ExecWriter::~ExecWriter() {
  _orderBookPtr = nullptr;
  _writerThread.join();
  writeBatch();
  _fileHandle.close();
}

std::string ExecWriter::formatTime(timestamp_t time_) {
  auto outTime = std::chrono::system_clock::to_time_t(time_);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&outTime), "%Y-%m-%d:%X");
  return ss.str();
}

void ExecWriter::write(const ExecReport::Ptr &message) {
  _fileHandle << LOG_NVP("ExecType", enum2str(message->execType()))
              << LOG_NVP("OrdStatus", enum2str(message->ordStatus()))
              << LOG_NVP("CumQty", message->cumQty())
              << LOG_NVP("OrdQty", message->ordQty())
              << LOG_NVP("LastPrice", message->lastPrice())
              << LOG_NVP("LastQty", message->lastQty())
              << LOG_NVP("OrderID", message->orderID())
              << LOG_NVP("Text", message->text())
              << LOG_NVP("TimeStamp", formatTime(message->timestamp()))
              << std::endl;
}

void ExecWriter::writeBatch() {
  _fileHandle.open(_fileLocation, std::ios::app);
  for (auto &message : _batch)
    write(message);
  _fileHandle.close();
  _batch.clear();
}

void ExecWriter::main() {
  while (_orderBookPtr) {
    ExecReport::Ptr message = _orderBookPtr->getExecMessage();
    if (message) {
      _batch.emplace_back(message);
    }
    if (_batch.size() >= _batchSize)
      writeBatch();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void ExecWriter::start() {
  _writerThread = std::thread(&ExecWriter::main, this);
}
