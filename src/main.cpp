#include <cstdlib>
#include <iostream>
#include <memory>

#include "ExecWriter.h"
#include "OrderBook.h"

void print_screen(const OrderBook::Ptr &orderBook) {
  system("clear");
  price_t ask = orderBook->bestAsk();
  price_t bid = orderBook->bestBid();
  qty_t askQty = orderBook->qtyAtLevel(Side::Sell, ask);
  qty_t bidQty = orderBook->qtyAtLevel(Side::Buy, bid);
  std::cout << "Bid: [" << bidQty << "@" << bid << "] Ask: [" << askQty << "@"
            << ask << "]\n";
}

int main() {

  auto orderBook = std::make_shared<OrderBook>("XYZ", 50.32);
  auto execWriter = std::make_shared<ExecWriter>(orderBook);

  std::unordered_map<int, std::unordered_map<int, Order::Ptr>> orders_;

  orderBook->start();
  execWriter->start();

  bool stop = false;
  print_screen(orderBook);

  while (not stop) {
    char request;
    int traderID;
    std::cout << "Enter TraderID: ";
    std::cin >> traderID;
    print_screen(orderBook);
    std::cout << "New (N) or Modify (M) or Exit (E): ";
    std::cin >> request;
    switch (request) {
    case ('N'): {
      print_screen(orderBook);
      price_t price;
      int qty;
      std::string side_st;
      std::cout << "Enter Side: ";
      std::cin >> side_st;
      print_screen(orderBook);
      auto side = str2enum<Side>(side_st.c_str());
      if (side == Side::Unknown) {
        std::cout << "Unknown order side: " << side_st << '\n';
        continue;
      }
      std::cout << "Price: ";
      std::cin >> price;
      print_screen(orderBook);
      std::cout << "Qty: ";
      std::cin >> qty;
      print_screen(orderBook);
      auto newOrder = std::make_shared<Order>(side, qty, price);
      orderBook->onOrderSingle(newOrder);
      orders_[traderID].emplace(std::pair(newOrder->orderID(), newOrder));
      break;
    }
    case ('M'): {
      print_screen(orderBook);
      auto &orderMap = orders_[traderID];
      std::cout << "Trader [" << traderID << "] ids=[";
      for (const auto &order : orderMap)
        std::cout << " " << order.first;
      std::cout << "] Enter OrderID to modfiy: ";
      int orderID;
      std::cin >> orderID;
      print_screen(orderBook);
      auto order = orderMap.find(orderID);
      if (order == orderMap.end()) {
        std::cout << "order: " << orderID << " not found\r";
        continue;
      }
      std::cout << "Enter Modified Qty (0 for cancel):";
      int qty;
      std::cin >> qty;
      print_screen(orderBook);
      auto modifiedOrder = std::make_shared<Order>(order->second->side(),
                                                   order->second->ordQty(),
                                                   order->second->price());
      modifiedOrder->setorderID(order->second->orderID());
      orderBook->onOrderCancelRequest(modifiedOrder);
      break;
    }
    case ('E'): {
      stop = true;
      break;
    }
    default: {
      std::cout << "Option " << request << " not recognised\r";
    }
    }
  }
  orderBook->stop();
}
