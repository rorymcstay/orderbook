#pragma once
#include <exception>
#include <queue>
#include <ctime>
#include <unordered_map>
#include <utility>
#include <vector>
#include <mutex>
#include <thread>

#include "Domain.h"
#include "Utils.h"


class OrderBook
{

    using OrderQueue=std::priority_queue<Order::Ptr, std::vector<Order::Ptr>, OrderCompare>;
    using Traders=std::unordered_map<int, Trader::Ptr>;
    using RootOrderMap=std::unordered_map<int, Order::Ptr>;

    OrderQueue                  _buyOrders;
    OrderQueue                  _sellOrders;
    RootOrderMap                _rootOrders;
    Traders                     _registeredTraders;
    price_t                      _tickSize;
    std::queue<ExecReport::Ptr> _execReports;
    int                         _oidSeed;
    std::string                 _symbol;
    price_t                      _closePrice;
    std::vector<qty_t>          _buyLevels;
    std::vector<qty_t>          _sellLevels;
    std::mutex                  _mutex;
    bool                        _open;
    std::thread                 _matchingThread;
    qty_t                       _tradedVolume;

public:
    using Ptr = std::shared_ptr<OrderBook>;
    OrderBook(std::string  symbol_, price_t closePrice_);
    ~OrderBook();

    void onOrderSingle(Order::Ptr& order_);
    void onOrderCancelRequest(const Order::Ptr &order_);

private:
    void matchingRoutine();
    void updateLevel(Side side_, price_t price_, qty_t qty_);
    bool isTraderRegistered(int traderID_);
    Trader::Ptr registerTrader(int traderID_);
    void acceptNewOrderRequest(const Order::Ptr& order_);
    void rejectNewOrderRequest(const Order::Ptr& order_, const std::string& reason_);
    void rejectCancelRequest(const Order::Ptr& order_, const std::string& reason_);
    Order::Ptr findRootOrder(int orderID_);
    void addExecReport(const ExecReport::Ptr& report_);
    void onAmendDown(const Order::Ptr &order_, qty_t newQty_);

    void onTrade(Order::Ptr& buyOrder_, Order::Ptr& sellOrder_, price_t crossPx_, qty_t crossQty_);
    void onCancel(const Order::Ptr &order_);
    void match();
    static bool canCross(const Order::Ptr& buyOrder_, const Order::Ptr& sellOrder_);
    Order::Ptr getLiveOrder(Side side_);
    OrderQueue& getOrderQueue(Side side_);
    bool isTickAligned(price_t price_) const;
    bool isValidPrice(price_t price_) const;
public:
    price_t tickSize() const { return _tickSize; }
    const std::string& symbol() const { return _symbol; }
    qty_t qtyAtLevel(Side side_, price_t price_) const;
    price_t bestAsk() const;
    price_t bestBid() const;
    ExecReport::Ptr getExecMessage();
    qty_t tradedVolume() const { return _tradedVolume; }

    void start();
    void stop();


};
