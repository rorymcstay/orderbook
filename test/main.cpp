#include <gtest/gtest.h>

#include "fwk/TestEnv.cpp"

TEST(OrderBook, smoke_test)
{
    TestEnv env("XYZ", 1.0);

    env << "NewOrder Price=1.0 OrdQty=100 Side=Buy TraderID=1" LN;
    ASSERT_EQ(env.orderBook()->qtyAtLevel(Side::Buy, 1.0), 100);
    env >> "ExecReport ExecType=New OrdStatus=New Price=1.0 Side=Buy OrdQty=100 LastQty=0 CumQty=0 LastPrice=0 OrderID=1" LN;
    env << "NewOrder Price=1.0 OrdQty=100 Side=Sell TraderID=2" LN;
    env >> "ExecReport ExecType=New OrdStatus=New Price=1.0 Side=Sell OrdQty=100 LastQty=0 CumQty=0 LastPrice=0 OrderID=2" LN;
    env >> "ExecReport ExecType=Trade OrdStatus=Filled Price=1.0 OrdQty=100 LastQty=100 CumQty=100 LastPrice=1.0 OrderID=2" LN;
    env >> "ExecReport ExecType=Trade OrdStatus=Filled Price=1.0 OrdQty=100 LastQty=100 CumQty=100 LastPrice=1.0 OrderID=1" LN;
    ASSERT_EQ(env.orderBook()->qtyAtLevel(Side::Buy, 1.0), 0);
    env >> "NONE" LN;
}

TEST(OrderBook, partial_fill)
{
    TestEnv env("XYZ", 1.0);

    env << "NewOrder Price=1.0 OrdQty=100 Side=Buy TraderID=1" LN;
    env >> "ExecReport ExecType=New OrdStatus=New Price=1.0 Side=Buy OrdQty=100 LastQty=0 CumQty=0 LastPrice=0 OrderID=1" LN;
    env << "NewOrder Price=1.0 OrdQty=50 Side=Sell TraderID=2" LN;
    env >> "ExecReport ExecType=New OrdStatus=New Price=1.0 Side=Sell OrdQty=50 LastQty=0 CumQty=0 LastPrice=0 OrderID=2" LN;
    env >> "ExecReport ExecType=Trade OrdStatus=Filled Price=1.0 OrdQty=50 LastQty=50 CumQty=50 LastPrice=1.0 OrderID=2" LN;
    env >> "ExecReport ExecType=Trade OrdStatus=PartiallyFilled Price=1.0 Side=Sell OrdQty=100 LastQty=50 CumQty=50 LastPrice=1.0 OrderID=1" LN;
}

TEST(OrderBook, multiple_levels)
{
    TestEnv env("XYZ", 1.0);
    env << "NewOrder Price=1.0 OrdQty=100 Side=Buy TraderID=1" LN;
    env >> "ExecReport ExecType=New OrdStatus=New Price=1.0 Side=Buy OrdQty=100 LastQty=0 CumQty=0 LastPrice=0 OrderID=1" LN;
    env << "NewOrder Price=1.1 OrdQty=50 Side=Sell TraderID=2" LN;
    env >> "ExecReport ExecType=New OrdStatus=New Price=1.1 Side=Sell OrdQty=50 LastQty=0 CumQty=0 LastPrice=0 OrderID=2" LN;
    env << "NewOrder Price=1.2 OrdQty=50 Side=Sell TraderID=3" LN;
    env << "NewOrder Price=0.9 OrdQty=50 Side=Buy TraderID=4" LN;
    env << "NewOrder Price=1.0 OrdQty=50 Side=Buy TraderID=4" LN;
    ASSERT_EQ(env.orderBook()->qtyAtLevel(Side::Buy, 0.9), 50);
    ASSERT_EQ(env.orderBook()->qtyAtLevel(Side::Buy, 1.0), 150);
    ASSERT_EQ(env.orderBook()->bestAsk(), 1.1);
    ASSERT_EQ(env.orderBook()->bestBid(), 1.0);
}

TEST(OrderBook, multiple_levels_close_price_greater_than_10)
{
    TestEnv env("XYZ", 50.32);
    env << "NewOrder Price=50.0 OrdQty=100 Side=Buy TraderID=1" LN;
    env << "NewOrder Price=51.0 OrdQty=50 Side=Sell TraderID=2" LN;
    env << "NewOrder Price=52.0 OrdQty=50 Side=Sell TraderID=3" LN;
    env << "NewOrder Price=49.0 OrdQty=50 Side=Buy TraderID=4" LN;
    env << "NewOrder Price=50.0 OrdQty=50 Side=Buy TraderID=4" LN;
    ASSERT_EQ(env.orderBook()->qtyAtLevel(Side::Buy, 49), 50);
    ASSERT_EQ(env.orderBook()->qtyAtLevel(Side::Buy, 50), 150);
    ASSERT_EQ(env.orderBook()->bestAsk(), 51);
    ASSERT_EQ(env.orderBook()->bestBid(), 50);
}

TEST(OrderBook, trader_cannot_exceed_100_messages_per_second)
{
    TestEnv env("XYZ", 50.32);
    for (int i(0); i < 100;)
    {
        env << "NewOrder Price=50.0 OrdQty=100 Side=Buy TraderID=1" LN;
        env >> ("ExecReport ExecType=New OrdStatus=New Price=50.0 Side=Buy OrdQty=100 LastQty=0 CumQty=0 LastPrice=0 OrderID="+ std::to_string(++i) + "" LN);
    }
    env << "NewOrder Price=50.0 OrdQty=100 Side=Buy TraderID=1" LN;
    env >> "ExecReport OrdStatus=Rejected ExecType=Reject OrderID=101 OrdQty=100 Side=Buy LastQty=0 CumQty=0 LastPrice=0 Price=50.0 Text=Message_rate_exceeded" LN;
}

TEST(OrderBook, trader_can_modify_quantity_down_on_order)
{
    TestEnv env("XYZ", 50.32);
    env << "NewOrder Price=50.0 OrdQty=100 Side=Buy TraderID=1" LN;
    env >> "ExecReport ExecType=New OrdStatus=New Price=50.0 Side=Buy OrdQty=100 LastQty=0 CumQty=0 LastPrice=0 OrderID=1" LN;
    ASSERT_EQ(env.orderBook()->qtyAtLevel(Side::Buy, 50), 100);
    env << "CancelOrder OrdQty=90 OrderID=1 Price=50.0 Side=Buy TraderID=1" LN;
    env >> "ExecReport ExecType=Replaced OrdQty=90 OrderID=1 Price=50.0 OrdStatus=New LastQty=0 CumQty=0 OrderID=1" LN;
    ASSERT_EQ(env.orderBook()->qtyAtLevel(Side::Buy, 50), 90);
    env >> "NONE" LN;
}



int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
