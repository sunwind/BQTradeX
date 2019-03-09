#ifndef _BQTRADEX_PRIVATE_H_
#define _BQTRADEX_PRIVATE_H_

#include <stdint.h>
#include <time.h>

/* Order Status
 */
#define ORDER_STATUS_Unknown    0
#define ORDER_STATUS_Inserted   1
#define ORDER_STATUS_Accepted   2
#define ORDER_STATUS_Traded     3
#define ORDER_STATUS_PartTraded 4
#define ORDER_STATUS_PartCancel 5
#define ORDER_STATUS_Canceled   6
#define ORDER_STATUS_Rejected   7

#define ORDER_DIRECTION_Buy     0
#define ORDER_DIRECTION_Sell    1

#define ORDER_PRICETYPE_Limit   0
#define ORDER_PRICETYPE_Market  1

#define CATEGORY_QUERY_FUND         0       // 资金
#define CATEGORY_QUERY_SHARES       1       // 股份
#define CATEGORY_QUERY_ORDERS       2       // 当日委托
#define CATEGORY_QUERY_TRADES       3       // 当日成交
#define CATEGORY_QUERY_PENDINGORDER 4       // 可撤单
#define CATEGORY_QUERY_HOLDERCODE   5       // 股东代码


typedef struct stTradeXLogonInfo
{
    char    m_IP[32];
    short   m_Port;
    short   m_YybID;
    char    m_Version[16];
    char    m_AccountNo[24];
    char    m_TradeAccount[24];
    char    m_JyPassword[24];
    char    m_TxPassword[24];

    int32_t m_ClientID;
    int32_t m_Logined;
}TradeXLogonInfo;

typedef struct stTradeXOrderInfo
{
    // input params
    int32_t m_ClientID;
    int32_t m_Category;         // 0-买入 1-卖出 2-融资买入 3-融资卖出
    int32_t m_PriceType;        // 0-limitorder, 1-市价，其它如五档转市/限价或撤销
    char    m_Gddm[24];
    char    m_StockName[24];
    char    m_Stock[12];
    float   m_Price;
    int32_t m_Quantity;

    // order params
    int32_t m_Direction;        // convert from category
    int32_t m_OrderID;
    int32_t m_Status;
    int32_t m_Filled;
    time_t  m_InsertTime;

    // trade params (we put them here)
    time_t  m_TradeTime;
    int32_t m_TradeId;
    float   m_TradePrice;

    // other params
    uint32_t m_Flags;
}TradeXOrderInfo;

typedef struct stTradeXPosition
{
    char    m_Stock[12];
    char    m_StockName[24];

    int32_t m_TotalSize;
    int32_t m_CoverableSize;
    int32_t  m_FrozenSize;
    double  m_MarketValue;
    double  m_CostValue;
    double  m_CostBasis;

    int32_t m_ClientID;
}TradeXPosition;


typedef struct stTradeXAccount
{
    char    m_AccountNo[24];
    char    m_TradeAccount[24];
    char    m_Currency[4];

    double  m_TotalAsset;
    double  m_UsableMoney;
    double  m_PositionValue;

    double  m_BalanceMoney;
    double  m_FrozenMoney;
    double  m_OnwayMoney;

    int32_t m_ClientID;
}TradeXAccount;


typedef struct stTradeXQuote
{
    char    m_Stock[12];
    char    m_StockName[24];
    char    m_Time[32];

    float   m_OpenPrice;
    float   m_HighPrice;
    float   m_LowPrice;
    float   m_ClosePrice;
    float   m_PrevClosePrice;
    float   m_LastPrice;
    float   m_UpperLimitPrice;
    float   m_LowerLimitPrice;

    int64_t m_Volume;
    double  m_Turnover;

}TradeXQuote;


//////////////////////////////////////////////////////////////////////////
/* some positions interfaces
 */
int StockPoistionUpdate(TradeXPosition* pos, int direction, int32_t volume, double price, bool istransaction);

int StockPositionUnfrozen(TradeXPosition* pos, int32_t volume);


#endif//_BQTRADEX_PRIVATE_H_
