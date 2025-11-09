#include <algorithm>
#include <memory>
#include <queue>
#include <thread>

#include "OrderBook.h"

OrderBook::OrderBook(std::string symbol_, price_t closePrice_)
    : _buyOrders(), _sellOrders(), _registeredTraders(), _tickSize(0.01),
      _oidSeed(0), _symbol(std::move(symbol_)), _closePrice(closePrice_),
      _buyLevels(std::round(1 / _tickSize) * 20, 0),
      _sellLevels(std::round(1 / _tickSize) * 20, 0), _open(false),
      _tradedVolume(0) {}

OrderBook::~OrderBook() {
  _open = false;
  // wait for matching to stop
  _matchingThread.join();
}

void OrderBook::start() {
  _open = true;
  _matchingThread = std::thread(&OrderBook::matchingRoutine, this);
}

void OrderBook::stop() { _open = false; }

void OrderBook::matchingRoutine() {
  INFO("Continuous trading start");
  while (_open) {
    match();
  }
  INFO("Continuous trading finish " << LOG_NVP("TotalVolume", _tradedVolume));
}

bool OrderBook::isTickAligned(price_t price_) const {
  if (((int)std::round(price_ * 100) % (int)std::round(tickSize() * 100)) != 0)
    return false;
  return true;
}

bool OrderBook::isValidPrice(price_t price_) const {
  if (std::abs(_closePrice - price_) > 10)
    return false;
  return true;
}

void OrderBook::updateLevel(Side side_, price_t price_, qty_t qty_) {
  auto &levels = side_ == Side::Buy ? _buyLevels : _sellLevels;
  // close and price must always be tick aligned, and price within 10 of
  // closePrice
  price_t normaliser = std::max(_closePrice - 10, 0.0);
  size_t level = (price_ - normaliser) / _tickSize;
  levels[level] += qty_;
}

qty_t OrderBook::qtyAtLevel(Side side_, price_t price_) const {
  auto &levels = side_ == Side::Buy ? _buyLevels : _sellLevels;
  if (price_ < 0 || price_ > levels.size())
    return 0;
  price_t normaliser = std::max(_closePrice - 10, 0.0);
  int index = (price_ - normaliser) / _tickSize;
  return levels[index];
}

Trader::Ptr OrderBook::registerTrader(int traderID_) {
  auto trader = std::make_shared<Trader>();
  _registeredTraders.emplace(std::pair(traderID_, trader));
  return trader;
}

bool OrderBook::isTraderRegistered(int traderID_) {
  if (_registeredTraders.find(traderID_) == _registeredTraders.end())
    return false;
  return true;
}

OrderBook::OrderQueue &OrderBook::getOrderQueue(Side side_) {
  return (side_ == Side::Buy) ? _buyOrders : _sellOrders;
}

void OrderBook::onOrderSingle(Order::Ptr &order_) {
  order_->setEntryTimeNow();
  order_->setorderID(++_oidSeed);
  std::lock_guard<decltype(_mutex)> lock(_mutex);
  auto &orderQueue = getOrderQueue(order_->side());
  if (not isTickAligned(order_->price())) {
    INFO("Order price is not a multiple of ticksize" << LOG_VAR(order_->price())
                                                     << LOG_VAR(_tickSize));
    rejectNewOrderRequest(order_, "Order_price_is_not_multiple_of_ticksize");
    return;
  }
  if (not isValidPrice(order_->price())) {
    INFO("Order price is not a multiple of within threshold (10) of"
         << LOG_VAR(_closePrice) << LOG_VAR(order_->price()));
    rejectNewOrderRequest(order_,
                          "Order_price_is_outside_threshold_of_closePrice");
    return;
  }
  auto traderID = order_->traderID();
  Trader::Ptr trader;
  if (not isTraderRegistered(traderID)) {
    trader = registerTrader(traderID);
  } else {
    trader = _registeredTraders[order_->traderID()];
  }
  if (trader->isRateExceeded()) {
    INFO("Message rate exceeded for "
         << LOG_NVP("traderID", order_->traderID()));
    rejectNewOrderRequest(order_, "Message_rate_exceeded");
    return;
  }
  acceptNewOrderRequest(order_);
  orderQueue.push(order_);
  updateLevel(order_->side(), order_->price(), order_->ordQty());
}

void OrderBook::rejectNewOrderRequest(const Order::Ptr &order_,
                                      const std::string &reason) {
  INFO("Rejecting new order request: "
       << LOG_NVP("OrderID", order_->orderID()) << LOG_VAR(reason)
       << LOG_NVP("TraderID", order_->traderID()));
  order_->setstatus(OrdStatus::Rejected);
  auto execReport = std::make_shared<ExecReport>(order_, ExecType::Reject);
  execReport->settext(reason);
  addExecReport(execReport);
}

void OrderBook::acceptNewOrderRequest(const Order::Ptr &order_) {
  INFO("Accepting new order request: " << LOG_NVP("OrderID", order_->orderID())
                                       << LOG_NVP("Side", order_->side())
                                       << LOG_NVP("Price", order_->price())
                                       << LOG_NVP("OrdQty", order_->ordQty()));
  order_->setstatus(OrdStatus::New);
  _rootOrders[order_->orderID()] = order_;
  addExecReport(std::make_shared<ExecReport>(order_, ExecType::New));
}

void OrderBook::addExecReport(const ExecReport::Ptr &execReport_) {
  _execReports.push(execReport_);
}

Order::Ptr OrderBook::findRootOrder(int orderID_) {
  if (_rootOrders.find(orderID_) == _rootOrders.end())
    return nullptr;
  else
    return _rootOrders[orderID_];
}

void OrderBook::onCancel(const Order::Ptr &order_) {
  order_->setstatus(OrdStatus::Cancelled);
  addExecReport(std::make_shared<ExecReport>(order_, ExecType::Cancel));
  _rootOrders.erase(order_->orderID());
  updateLevel(order_->side(), order_->price(), order_->leavesQty());
}

void OrderBook::onAmendDown(const Order::Ptr &order_, qty_t newQty_) {
  qty_t oldQty = order_->ordQty();
  order_->setordQty(newQty_);
  addExecReport(std::make_shared<ExecReport>(order_, ExecType::Replaced));
  updateLevel(order_->side(), order_->price(), newQty_ - oldQty);
}

void OrderBook::onOrderCancelRequest(const Order::Ptr &order_) {
  std::lock_guard<decltype(_mutex)> lock(_mutex);
  auto traderID = order_->traderID();
  if (not isTraderRegistered(traderID)) {
    rejectCancelRequest(order_, "Trader_not_registered.");
    return;
  }
  if (_registeredTraders[traderID]->isRateExceeded()) {
    rejectCancelRequest(order_, "Message_rate_exceeded");
  }

  auto originalOrder = findRootOrder(order_->orderID());
  if (!originalOrder) {
    rejectCancelRequest(order_, "Order_not_found.");
    return;
  }
  qty_t newQty = order_->ordQty();
  qty_t oldQty = originalOrder->cumQty();
  if (newQty < oldQty) {
    rejectCancelRequest(originalOrder, "Quantity_amend_up_is_not_allowed");
    return;
  }

  if (newQty == 0) {
    onCancel(originalOrder);
  } else if (newQty < order_->cumQty()) {
    rejectCancelRequest(originalOrder, "Too_late_to_cancel");
    onCancel(originalOrder);
  } else {
    onAmendDown(originalOrder, newQty);
  }
}

bool OrderBook::canCross(const Order::Ptr &buy_, const Order::Ptr &sell_) {
  if (greater_equal(buy_->price(), sell_->price()))
    return true;
  else
    return false;
}

ExecReport::Ptr OrderBook::getExecMessage() {
  std::lock_guard<decltype(_mutex)> lock(_mutex);
  if (_execReports.empty())
    return nullptr;
  auto execReport = _execReports.front();
  _execReports.pop();
  return execReport;
}

void OrderBook::onTrade(Order::Ptr &buyOrder_, Order::Ptr &sellOrder_,
                        price_t crossPx_, qty_t crossQty_) {
  INFO("Trade: " << LOG_NVP("Price", crossPx_)
                 << LOG_NVP("Quantity", crossQty_));
  buyOrder_->setlastPrice(crossPx_);
  sellOrder_->setlastPrice(crossPx_);
  buyOrder_->setlastQty(crossQty_);
  sellOrder_->setlastQty(crossQty_);
  buyOrder_->setstatus(OrdStatus::PartiallyFilled);
  sellOrder_->setstatus(OrdStatus::PartiallyFilled);
  if (buyOrder_->leavesQty() == 0) {
    // finalise order
    buyOrder_->setstatus(OrdStatus::Filled);
    _buyOrders.pop();
  }
  if (sellOrder_->leavesQty() == 0) {
    // finalise order
    sellOrder_->setstatus(OrdStatus::Filled);
    _sellOrders.pop();
  }
  // send exec reports
  auto execReport = std::make_shared<ExecReport>(sellOrder_, ExecType::Trade);
  addExecReport(execReport);
  execReport = std::make_shared<ExecReport>(buyOrder_, ExecType::Trade);
  addExecReport(execReport);
  updateLevel(Side::Buy, buyOrder_->price(), -crossQty_);
  updateLevel(Side::Sell, sellOrder_->price(), -crossQty_);
  _tradedVolume += crossQty_;
}

Order::Ptr OrderBook::getLiveOrder(Side side_) {
  auto &orderQueue = getOrderQueue(side_);
  Order::Ptr order;
  while (!orderQueue.empty()) {
    order = orderQueue.top();
    if (not order)
      break;
    if (order->isCancelled()) {
      orderQueue.pop();
      continue;
    }
    return order;
  }
  return nullptr;
}

void OrderBook::match() {
  std::lock_guard<decltype(_mutex)> lock(_mutex);
  if (_buyOrders.empty() || _sellOrders.empty()) {
    return;
  }
  auto buyOrder = getLiveOrder(Side::Buy);
  auto sellOrder = getLiveOrder(Side::Sell);
  if (!buyOrder || !sellOrder)
    return;
  if (canCross(buyOrder, sellOrder)) {
    auto crossQty = std::min(buyOrder->leavesQty(), sellOrder->leavesQty());
    auto crossPx = std::min(buyOrder->price(), sellOrder->price());
    onTrade(buyOrder, sellOrder, crossPx, crossQty);
  }
}

void OrderBook::rejectCancelRequest(const Order::Ptr &order_,
                                    const std::string &reason_) {
  auto execReport =
      std::make_shared<ExecReport>(order_, ExecType::CancelReject);
  execReport->settext(reason_);
  addExecReport(execReport);
}

price_t OrderBook::bestBid() const {
  for (size_t i(_buyLevels.size() - 1); i >= 0; i--) {
    if (_buyLevels[i] != 0)
      return (i * _tickSize) + std::max(_closePrice - 10, 0.0);
  }
  return -1;
}

price_t OrderBook::bestAsk() const {
  for (size_t i(0); i < _sellLevels.size(); i++) {
    if (_sellLevels[i] != 0)
      return (i * _tickSize) + std::max(_closePrice - 10, 0.0);
  }
  return -1;
}
