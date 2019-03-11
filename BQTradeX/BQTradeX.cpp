#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "BQTradeX.h"
#include "BQTradeXPrivate.h"

#include <string>
#include <vector>
#include <list>
#include <map>
#include <mutex>
#include <assert.h>
using namespace std;

/* 设计（先支持单账号，后支持多账号）：
 * 每次登录，产生一个ClientId
 * 每个订单/成交也对应于一个ID
 * 记录所有的下单，若为市价单，则立即成交，并更新成交数量，订单状态，
 * 成交/取消后的订单，在挂单查询中不能查到,当日委托可查询到所有订单，当日成交则只查询到已成交的单子
 * （以下类操作，可封装成独立的模块，方便使用或借鉴）
 * TODO: 下平仓单时，应冻结持仓中的数量
 * TODO: 撤销时，应该取消冻结持仓中的数量
 * TODO: 成交时，更新账户持仓（分买入或卖出），并更新账户资金使用，
 * 行情查询，当前只有两支股票的数据，未来可通过从文件加载等方式
 */

static const char* version = "0.1.0";

static std::mutex g_Mutex;

#define BQTRADEX_SCOPE_LOCK std::lock_guard<mutex> lk(g_Mutex)

static bool _out2stdout = false;

static int32_t _loadconfig = 0;
static const char* config_file = "init_tradex_data.json";
static double _defaultCash = 500000;


static int32_t g_ClientId = 1;
static int32_t g_OrderId = 1;
static int32_t g_MatchId = 1;

typedef vector<TradeXLogonInfo> LogonInfoContainer;
static LogonInfoContainer g_LogonInfoVec;

typedef list<TradeXOrderInfo> OrderContainer;
static OrderContainer g_Orders;

typedef map<string, TradeXQuote> QuoteMap;
QuoteMap g_Quotes;

typedef map<string, TradeXPosition> PositionMap;
PositionMap g_Positions;


class ClientManager
{
public:
    ClientManager(int32_t aClientId) : m_LogonInfo(), m_Orders(), m_Positions()
    {
        m_LogonInfo.m_ClientID = aClientId;
    }
    ~ClientManager() {}

    int32_t ClientID() {
        return m_LogonInfo.m_ClientID;
    }
    const char* AccountNo() {
        return m_LogonInfo.m_AccountNo;
    }

    TradeXLogonInfo* LogonInfo() {
        return &m_LogonInfo;
    }

    TradeXAccount* GetAccount() {
        return &m_TradingAccount;
    }

    OrderContainer& Orders() {
        return m_Orders;
    }

    PositionMap& Positoins() {
        return m_Positions;
    }

    TradeXPosition* GetPosition(const std::string& aStockCode, bool aCreateIfNone = false)
    {
        TradeXPosition* lpPostion = NULL;
        if (m_Positions.count(aStockCode))
        {
            lpPostion = &m_Positions[aStockCode];
        }
        else if (aCreateIfNone)
        {
            TradeXPosition lPosition = { 0 };
            strncpy(lPosition.m_Stock, aStockCode.c_str(), sizeof(lPosition.m_Stock));
            strncpy(lPosition.m_StockName, aStockCode.c_str(), sizeof(lPosition.m_StockName));
            lPosition.m_ClientID = ClientID();
            m_Positions.insert(std::make_pair(aStockCode, lPosition));
            lpPostion = &m_Positions[aStockCode];
        }

        return lpPostion;
    }

    void InitAccount(const char* apAccountNo)
    {
        TradeXAccount& lAccount = *GetAccount();
        memset(&lAccount, 0, sizeof(lAccount));
        strcpy(lAccount.m_AccountNo, apAccountNo);
        strcpy(lAccount.m_TradeAccount, apAccountNo);
        strcpy(lAccount.m_Currency, "CNY");
        lAccount.m_TotalAsset = _defaultCash;
        lAccount.m_UsableMoney = _defaultCash;
        lAccount.m_PositionValue = 0;
        lAccount.m_BalanceMoney = _defaultCash;
        lAccount.m_FrozenMoney = 0.0;
        lAccount.m_OnwayMoney = 25000;
    }
    void InitPosition()
    {
        if (strcmp(m_TradingAccount.m_AccountNo, "1650002099") == 0)
        {
#if 0
            TradeXPosition& lPosition = *GetPosition("600569", true);
            memset(&lPosition, 0, sizeof(lPosition));
            lPosition.m_ClientID = ClientID();
            strcpy(lPosition.m_Stock, "600569");
            strcpy(lPosition.m_StockName, "安阳钢铁");
            lPosition.m_TotalSize = 6400;
            lPosition.m_CoverableSize = 6400;
            lPosition.m_MarketValue = 4.13 * 6400;
            lPosition.m_CostBasis = 4.5;

            TradeXAccount& lAccount = *GetAccount();
            lAccount.m_PositionValue = lPosition.m_MarketValue;
            lAccount.m_UsableMoney = 800000;
            lAccount.m_BalanceMoney = 800000;
            lAccount.m_TotalAsset = lAccount.m_BalanceMoney + lPosition.m_MarketValue;
#endif//0
        }
    }

public:
    TradeXLogonInfo m_LogonInfo;
    TradeXAccount   m_TradingAccount;
    OrderContainer  m_Orders;
    PositionMap     m_Positions;
};

typedef map<int32_t, ClientManager*> ClientsMap;
static ClientsMap g_clients;
static ClientManager* GetClientManager(int32_t aClientID, bool aCreateIfNone)
{
    if (g_clients.count(aClientID)) {
        return g_clients[aClientID];
    }
    else if (aCreateIfNone) {
        ClientManager* lpClient = new ClientManager(aClientID);
        g_clients[aClientID] = lpClient;
        return lpClient;
    }
    return NULL;
}

//////////////////////////////////////////////////////////////////////////

static void LogDebug(int level, const char* fmt, ...);

static TradeXQuote* LoadQuote(const char* apZqdm);

static int ConvertTime(char aTimeBuf[], time_t aTime);
static const char* GetStatusDesc(int aStatus);
static const char* GetDirectionDesc(int aDirection);
static const char* GetPriceTypeDesc(int aPriceType);

static const char* GetAccountNo(int32_t aClientId);
static ClientManager* GetClientByAccntNo(const char* apAccountNo);

static int32_t NextClientId();
static int32_t NextOrderId();
static int32_t NextMatchId();

static void _UpdateInfoWhenMatched(double lastprice, TradeXOrderInfo* apOrder, TradeXPosition* apPosition, TradeXAccount* apAccount);
static void _MatchOrder(const char* apStockCode, double lastprice, int lastvol, TradeXOrderInfo* apOrder, ClientManager* apClient);

//////////////////////////////////////////////////////////////////////////
/* api implements */

void BQTRADEX_API WINAPI OpenTdx()
{
    LogDebug(0, "-------- OpenTdx %s --------", version);
    if (!_loadconfig)
    {
        _loadconfig = 1;
        extern int ReadConfig(const char*, double*);
        ReadConfig(config_file, &_defaultCash);
    }
    LoadQuote(NULL);
}

void BQTRADEX_API WINAPI CloseTdx()
{
    //
}

int BQTRADEX_API WINAPI Logon(
    const char* pszIP,
    short nPort,
    const char* pszVersion,
    short nYybID,
    const char* pszAccountNo,
    const char* pszTradeAccount,
    const char* pszJyPassword,
    const char* pszTxPassword,
    char* pszErrInfo)
{
    BQTRADEX_SCOPE_LOCK;

    if (pszErrInfo) {
        pszErrInfo[0] = 0;
    }

    ClientManager* lpClient = GetClientByAccntNo(pszAccountNo);
    if (lpClient && lpClient->LogonInfo()->m_Logined)
    {
        LogDebug(0, "Logon:%s already -->> clientId:%d\n", pszAccountNo, lpClient->ClientID());
        return lpClient->ClientID();
    }

    TradeXLogonInfo lLogonInfo;
    memset(&lLogonInfo, 0, sizeof(lLogonInfo));
    lLogonInfo.m_Port = nPort;
    lLogonInfo.m_YybID = nYybID;
    strncpy(lLogonInfo.m_IP, pszIP, sizeof(lLogonInfo.m_IP));
    strncpy(lLogonInfo.m_Version, pszIP, sizeof(lLogonInfo.m_IP));
    strncpy(lLogonInfo.m_AccountNo, pszAccountNo, sizeof(lLogonInfo.m_AccountNo));
    strncpy(lLogonInfo.m_JyPassword, pszJyPassword, sizeof(lLogonInfo.m_JyPassword));
    strncpy(lLogonInfo.m_TxPassword, pszTxPassword, sizeof(lLogonInfo.m_TxPassword));

    int32_t lClientId = NextClientId();
    lLogonInfo.m_ClientID = lClientId;
    lLogonInfo.m_Logined = 1;

    // save into client object
    lpClient = GetClientManager(lClientId, true);
    *lpClient->LogonInfo() = lLogonInfo;
    lpClient->InitAccount(pszAccountNo);
    lpClient->InitPosition();   // call after InitAccount

    LogDebug(0, "Logon:%s -->> clientId:%d\n", pszAccountNo, lClientId);
    return lClientId;
}

void BQTRADEX_API WINAPI Logoff(int nClientID)
{
    BQTRADEX_SCOPE_LOCK;

    ClientManager* lpClient;
    lpClient = GetClientManager(nClientID, false);
    if (lpClient)
    {
        lpClient->LogonInfo()->m_Logined = 0;
        LogDebug(0, "Logoff:%s, clientid:%d\n", lpClient->AccountNo(), nClientID);
    }
}

bool BQTRADEX_API WINAPI IsConnectOK(int nClientID)
{
    BQTRADEX_SCOPE_LOCK;

    ClientManager* lpClient;
    lpClient = GetClientManager(nClientID, false);
    if (lpClient)
    {
        if (lpClient->LogonInfo()->m_Logined) {
            return true;
        }
    }
    return false;
}

void BQTRADEX_API WINAPI QueryData(
    int nClientID,
    int nCategory,
    char* pszResult,
    char* pszErrInfo)
{
    if (!IsConnectOK(nClientID))
    {
        sprintf(pszErrInfo, "QueryData not connect for %d\n", nClientID);
        return;
    }

    BQTRADEX_SCOPE_LOCK;

    if (pszResult) {
        pszResult[0] = 0;
    }
    if (pszErrInfo) {
        pszErrInfo[0] = 0;
    }

    ClientManager* lpClient = GetClientManager(nClientID, false);
    if (!lpClient)
    {
        sprintf(pszErrInfo, "QueryData not find account no for %d\n", nClientID);
        return;
    }

    char lBuffer[4090] = "";
    int  lLength = 0;
    if (nCategory == CATEGORY_QUERY_FUND)
    {
        LogDebug(0, "QueryData(0fund) for clientId:%d\n", nClientID);

        // make head line
        //strcpy(lBuffer, "TotalAsset\tUsableMoney\tPositionValue\tBalance\tFrozen\n");
        strcpy(lBuffer, "资金账号\t币种\t总资产\t可用资金\t最新市值\t资金余额\t冻结资金\t在途资金\n");
        lLength = (int)strlen(lBuffer);

        // make a fake line
        TradeXAccount* lpAccount = lpClient->GetAccount();
        sprintf(lBuffer + lLength, "%s\t%s\t%.1lf\t%.1lf\t%.2lf\t%.1lf\t%.2lf\t%.1lf\n", lpAccount->m_AccountNo, lpAccount->m_Currency,
            lpAccount->m_TotalAsset, lpAccount->m_UsableMoney, lpAccount->m_PositionValue, lpAccount->m_BalanceMoney, lpAccount->m_FrozenMoney, lpAccount->m_OnwayMoney);
        lLength = (int)strlen(lBuffer);

        strncpy(pszResult, lBuffer, lLength);
        pszResult[lLength] = 0;
    }
    else if (nCategory == CATEGORY_QUERY_SHARES)
    {
        LogDebug(0, "QueryData(1shares) for clientId:%d\n", nClientID);

        //strcpy(lBuffer, "证券代码\t证券名称\t证券数量\t持仓量\t可卖数量\t当前价\t最新市值\t成本价\t浮动盈亏\t盈亏比例(%)\t"
        //    "帐号类别\t资金账号\t股东代码\t交易所名称\t买卖标志\t投保标志\t今买数量\t今卖数量\t保证金\t保留信息\n");
        strcpy(lBuffer, "证券代码\t证券名称\tDirectin\t证券数量\t可卖数量\t最新市值\t成本价\t保留信息\n");
        lLength = (int)strlen(lBuffer);

        PositionMap& lPositions = lpClient->Positoins();
        for (PositionMap::iterator iter = lPositions.begin(); iter != lPositions.end(); ++iter)
        {
            TradeXPosition* lpPos = &iter->second;
            if (lpPos->m_TotalSize == 0) {
                continue;
            }

            lLength += sprintf(lBuffer + lLength, "%s\t%s\t0\t%d\t%d\t%.2lf\t%.3lf\ta\n", lpPos->m_Stock, lpPos->m_StockName,
                lpPos->m_TotalSize, lpPos->m_CoverableSize, lpPos->m_MarketValue, lpPos->m_CostBasis);

            LogDebug(0, "position: %s,%d,%.2lf\n", lpPos->m_Stock, lpPos->m_TotalSize, lpPos->m_CostBasis);
        }

        strncpy(pszResult, lBuffer, lLength);
        pszResult[lLength] = 0;
    }
    else if (nCategory == CATEGORY_QUERY_ORDERS)
    {
        /* query all orders of this trading day, 
         * FIXME:每个券商的返回的格式可能不一样
         */
        strcpy(lBuffer, "证券代码\t证券名称\t买卖标志\t委托类别\t委托价格\t委托数量\t状态说明\t成交数量\t委托编号\t委托时间\n");
        //strcpy(lBuffer, "证券代码\t证券名称\t买卖标志\t委托价格\t委托数量\t状态说明\t成交数量\t委托编号\t委托时间");
        lLength = (int)strlen(lBuffer);

        OrderContainer& lOrders = lpClient->Orders();

        int lCount = 0;
        OrderContainer::iterator iter = lOrders.begin();
        for (; iter != lOrders.end(); ++iter)
        {
            if (iter->m_ClientID != nClientID)
                continue;

            ++lCount;
            char lOrderTime[32] = "";
            ConvertTime(lOrderTime, iter->m_InsertTime);
            const char* lpStatusDesc = GetStatusDesc(iter->m_Status);
            const char* lpDirectionDesc = GetDirectionDesc(iter->m_Direction);
            const char* lpCategory = GetPriceTypeDesc(iter->m_PriceType);
            lLength += sprintf(lBuffer + lLength, "%s\t%s\t%s\t%s\t%.2f\t%d\t%s\t%d\t%04d\t%s\n", iter->m_Stock, iter->m_StockName,
            //lLength += sprintf(lBuffer + lLength, "%s\t%s\t%s\t%.2f\t%d\t%s\t%d\t%04d\t%s\n", iter->m_Stock, iter->m_StockName,
                lpDirectionDesc, lpCategory, iter->m_Price, iter->m_Quantity, lpStatusDesc, iter->m_Filled, iter->m_OrderID, lOrderTime);
        }

        if (lCount == 0)
        {
            // no orders ...
            LogDebug(0, "QueryData(2order) no orders for clientId:%d\n", nClientID);
            sprintf(pszErrInfo, "QueryData(order) no orders for clientId:%d\n", nClientID);
        }
        else
        {
            LogDebug(0, "QueryData(2order) have %d orders for clientId:%d, length:%d\n", lCount, nClientID, lLength);
            //LogDebug(0, lBuffer);
        }
        strncpy(pszResult, lBuffer, lLength);
        pszResult[lLength] = 0;
    }
    else if (nCategory == CATEGORY_QUERY_TRADES)
    {
        //strcpy(lBuffer, "StockCode\tDirection\tTradePrice\tTradeVolume\tTradeMoney\n");
        strcpy(lBuffer, "证券代码\t买卖标志\t成交价格\t成交数量\t成交金额\t成交编号\t成交时间\t委托类别\t委托编号\t股东代码\n");
        lLength = (int)strlen(lBuffer);

        OrderContainer& lOrders = lpClient->Orders();
        LogDebug(0, "QueryData(3trades) for clientId:%d, count:%d\n", nClientID, lOrders.size());

        int lCount = 0;
        OrderContainer::iterator iter = lOrders.begin();
        for (; iter != lOrders.end(); ++iter)
        {
            if (iter->m_ClientID != nClientID)
                continue;

            if (iter->m_Status == ORDER_STATUS_Traded || iter->m_Status == ORDER_STATUS_PartTraded)
            {
                ++lCount;
                char lTradeTime[32] = "";
                ConvertTime(lTradeTime, iter->m_TradeTime);
                const char* lpDirectionDesc = GetDirectionDesc(iter->m_Direction);
                const char* lpPriceDesc = GetPriceTypeDesc(iter->m_PriceType);
                lLength += sprintf(lBuffer + lLength, "%s\t%s\t%.2f\t%d\t%2.lf\t%05d\t%s\t%s\t%04d\t%s\n", iter->m_Stock, lpDirectionDesc,
                    iter->m_TradePrice, iter->m_Filled, (iter->m_TradePrice * iter->m_Filled), iter->m_TradeId, lTradeTime, lpPriceDesc, iter->m_OrderID, iter->m_Gddm);
            }
        }

        strncpy(pszResult, lBuffer, lLength);
        pszResult[lLength] = 0;
    }
    else if (nCategory == CATEGORY_QUERY_PENDINGORDER)
    {
        /* only query pending orders */
        strcpy(lBuffer, "证券代码\t证券名称\t买卖标志\t委托类别\t委托价格\t委托数量\t成交数量\t委托编号\t委托时间\n");
        lLength = (int)strlen(lBuffer);

        OrderContainer& lOrders = lpClient->Orders();

        int lCount = 0;
        OrderContainer::iterator iter = lOrders.begin();
        for (; iter != lOrders.end(); ++iter)
        {
            if (iter->m_ClientID != nClientID)
                continue;

            if (iter->m_Status != ORDER_STATUS_Inserted && 
                iter->m_Status != ORDER_STATUS_Accepted && 
                iter->m_Status != ORDER_STATUS_PartTraded)
            {
                continue;
            }

            ++lCount;
            char lOrderTime[32] = "";
            ConvertTime(lOrderTime, iter->m_InsertTime);
            const char* lpStatusDesc = GetStatusDesc(iter->m_Status);
            const char* lpDirectionDesc = GetDirectionDesc(iter->m_Direction);
            lLength += sprintf(lBuffer + lLength, "%s\t%s\t%s\t%d\t%.2f\t%d\t%d\t%04d\t%s\n", iter->m_Stock, iter->m_StockName,
                lpDirectionDesc, iter->m_Category, iter->m_Price, iter->m_Quantity, iter->m_Filled, iter->m_OrderID, lOrderTime);
        }

        if (lCount == 0)
        {
            // no orders ...
            LogDebug(0, "QueryData(4pendingorder) no pending orders for clientId:%d\n", nClientID);
        }
        else
        {
            LogDebug(0, "QueryData(4pendingorder) have %d pending orders for clientId:%d, length:%d\n", lCount, nClientID, lLength);
            //LogDebug(0, lBuffer);
        }
        strncpy(pszResult, lBuffer, lLength);
        pszResult[lLength] = 0;
    }
    else if (nCategory == 5/*CATEGORY_QUERY_HOLDERCODE*/)
    {
        LogDebug(0, "QueryData(5holder) for clientId:%d\n", nClientID);

        strcpy(lBuffer, "股东代码\t股东名称\t帐号类别\t保留信息\n");
        TradeXAccount* lpAccount = lpClient->GetAccount();
        if (strcmp("1650002099", lpAccount->m_AccountNo) == 0)
        {
            lLength = (int)strlen(lBuffer);
            strcpy(lBuffer + lLength, "A195591112\t1650002099\t0\t1\n");
            lLength = (int)strlen(lBuffer);
            strcpy(lBuffer + lLength, "0239913472\t1650002099\t0\t3\n");
            lLength = (int)strlen(lBuffer);
        }
        else
        {
            lLength = (int)strlen(lBuffer);
            lLength += sprintf(lBuffer + lLength, "A19559111%d\t%s\t0\t2\n", nClientID % 10, lpAccount->m_AccountNo);
            lLength += sprintf(lBuffer + lLength, "023991347%d\t%s\t0\t2\n", nClientID % 10, lpAccount->m_AccountNo);
        }

        strncpy(pszResult, lBuffer, lLength);
        pszResult[lLength] = 0;
    }
    else
    {
        sprintf(pszErrInfo, "QueryData no data for category:%d!!\n", nCategory);
        *pszResult = '\0';
    }
}

/* Internal process order input */
static bool _InputOrder(
    int nClientID,
    int nCategory,
    int nPriceType,
    const char* pszGddm,
    const char* pszZqdm,
    float fPrice,
    int   nQuantity,
    char* pszResult,
    char* pszErrInfo)
{
    ClientManager* lpClient = GetClientManager(nClientID, false);
    assert(lpClient != NULL);

    TradeXAccount* lpAccount = lpClient->GetAccount();

    // save the order, and got one reply line
    TradeXOrderInfo lOrder;
    memset(&lOrder, 0, sizeof(lOrder));

    lOrder.m_ClientID = nClientID;
    lOrder.m_Category = nCategory;
    lOrder.m_PriceType = nPriceType;
    strncpy(lOrder.m_Gddm, pszGddm, sizeof(lOrder.m_Gddm));
    strncpy(lOrder.m_StockName, pszZqdm, sizeof(lOrder.m_StockName));
    strncpy(lOrder.m_Stock, pszZqdm, sizeof(lOrder.m_Stock));
    lOrder.m_Price = fPrice;
    lOrder.m_Quantity = nQuantity;

    lOrder.m_OrderID = NextOrderId();
    lOrder.m_InsertTime = time(0);

    if (nCategory == 0 || nCategory == 2)
        lOrder.m_Direction = ORDER_DIRECTION_Buy;
    else
        lOrder.m_Direction = ORDER_DIRECTION_Sell;

    lOrder.m_Status = ORDER_STATUS_Accepted;

    TradeXPosition* lpPos = lpClient->GetPosition(pszZqdm, true);
    if (lOrder.m_Direction == ORDER_DIRECTION_Sell && lpPos->m_CoverableSize < nQuantity)
    {
        LogDebug(0, "_InputOrder no position to cover client:%d, Zqdm:%s, CoverbaleSize:%d\n", nClientID, pszZqdm, lpPos->m_CoverableSize);
        sprintf(pszErrInfo, "SendOrder failed since no position to cover for client:%d, stock:%s, category:%d\n", nClientID, pszZqdm, nCategory);
        return false;
    }
    StockPoistionUpdate(lpPos, lOrder.m_Direction, nQuantity, fPrice, false);

    // !! special treat as market order here for easily debug test !!
    int lOriginalPriceType = lOrder.m_PriceType;
    static int s_DebugIndex = 0;
    if (s_DebugIndex++ % 2 == 0)
        lOrder.m_PriceType = ORDER_PRICETYPE_Market;

    // not enough money
    if (lpAccount->m_UsableMoney < (fPrice * nQuantity))
    {
        LogDebug(0, "_InputOrder no enough money client:%d, Zqdm:%s, price:%.2lf * vol:%d = %.2lf > useable:%.2lf\n", nClientID,
            pszZqdm, fPrice, nQuantity, fPrice * nQuantity, lpAccount->m_UsableMoney);
        sprintf(pszErrInfo, "SendOrder failed since no enough money for client:%d, stock:%s, category:%d\n", nClientID, pszZqdm, nCategory);
        return false;
    }

    // try match order if market order (0 is limit order), deal immediately
    _MatchOrder(pszZqdm, fPrice, nQuantity, &lOrder, lpClient);

    lOrder.m_PriceType = lOriginalPriceType;

    lpClient->Orders().push_back(lOrder);

    // g_Orders will be deprecated
    g_Orders.push_back(lOrder);

    // reply line
    char lInsertTime[32] = "";
    ConvertTime(lInsertTime, lOrder.m_InsertTime);
    sprintf(pszResult, "%d\t%s\n", lOrder.m_OrderID, lInsertTime);

    return true;
}

void BQTRADEX_API WINAPI SendOrder(
    int nClientID,
    int nCategory,
    int nPriceType,
    const char* pszGddm,
    const char* pszZqdm,
    float fPrice,
    int   nQuantity,
    char* pszResult,
    char* pszErrInfo)
{
    /*
    Category 表示委托的种类
    0买入
    1卖出
    2融资买入
    3融券卖出
    4买券还券
    5卖券还款
    6现券还券
    PriceType 表示报价方式
    0上海限价委托 深圳限价委托
    1(市价委托)深圳对方最优价格
    2(市价委托)深圳本方最优价格
    3(市价委托)深圳即时成交剩余撤销
    4(市价委托)上海五档即成剩撤 深圳五档即成剩撤
    5(市价委托)深圳全额成交或撤销
    6(市价委托)上海五档即成转限价
     */
    if (!IsConnectOK(nClientID))
    {
        sprintf(pszErrInfo, "SendOrder not connect for %d\n", nClientID);
        return;
    }

    BQTRADEX_SCOPE_LOCK;

    if (pszResult) {
        pszResult[0] = 0;
    }
    if (pszErrInfo) {
        pszErrInfo[0] = 0;
    }

    const char* lpDirectionDesc = GetDirectionDesc(nCategory);
    const char* lpPriceTypeDesc = GetPriceTypeDesc(nPriceType);
    LogDebug(0, "SendOrder: clienid:%d,code:%s,price:%.2f,vol:%d,direction:%s,pricetype:%s\n", nClientID, pszZqdm, fPrice, nQuantity, lpDirectionDesc, lpPriceTypeDesc);

    int lLength = 0;
    lLength = sprintf(pszResult + lLength, "委托号\t委托时间\n");
    if (!_InputOrder(nClientID, nCategory, nPriceType, pszGddm, pszZqdm, fPrice, nQuantity, pszResult + lLength, pszErrInfo))
    {
        // sprintf(pszErrInfo, "SendOrder place failed for client:%d, stock:%s, category:%d\n", nClientID, pszZqdm, nCategory);
        memset(pszResult, 0, lLength);
    }
}

void BQTRADEX_API WINAPI CancelOrder(
    int nClientID,
    const char* pszExchangeID,
    const char* pszhth,
    char* pszResult,
    char* pszErrInfo)
{
    if (!IsConnectOK(nClientID))
    {
        sprintf(pszErrInfo, "CancelOrder not connect for %d\n", nClientID);
        return;
    }

    BQTRADEX_SCOPE_LOCK;

    if (pszResult) {
        pszResult[0] = 0;
    }
    if (pszErrInfo) {
        pszErrInfo[0] = 0;
    }

    ClientManager* lpClient = GetClientManager(nClientID, false);
    assert(lpClient != NULL);

    int lOrderId = atoi(pszhth);
    if (lOrderId <= 0)
    {
        sprintf(pszErrInfo, "CancelOrder invalid orderid:%s\n", pszhth);
        return;
    }

    int  lLength = 0;
    char lBuffer[4090] = "";
    strcpy(lBuffer, "委托号\t状态\n");
    lLength = (int)strlen(lBuffer);

    OrderContainer& lOrders = lpClient->Orders();

    bool lFind = false;
    OrderContainer::iterator iter = lOrders.begin();
    for (; iter != lOrders.end(); ++iter)
    {
        // find the clientid & orderid
        if (iter->m_ClientID == nClientID && iter->m_OrderID == lOrderId)
        {
            lFind = true;

            // only limit order could be canceled currently
            if (iter->m_PriceType == ORDER_PRICETYPE_Limit)
            {
                // just mark as canceled
                iter->m_Status = ORDER_STATUS_Canceled;

                const char* lpStatus = GetStatusDesc(iter->m_Status);
                lLength += sprintf(lBuffer, "%s\t%s\n", pszhth, lpStatus);
                strncpy(pszResult, lBuffer, lLength);
                pszResult[lLength] = 0;

                TradeXPosition* lpPos = lpClient->GetPosition(iter->m_Stock, false);
                if (iter->m_Direction == ORDER_DIRECTION_Sell && lpPos)
                {
                    StockPositionUnfrozen(lpPos, iter->m_Quantity);
                }
                return;
            }
        }
    }

    // here, not find the order
    if (!lFind)
    {
        sprintf(pszErrInfo, "CancelOrder not find the orderid:%s\n", pszhth);
    }
    else
    {
        sprintf(pszErrInfo, "CancelOrder order has traded orderid:%s\n", pszhth);
    }
    *pszResult = '\0';
}

void BQTRADEX_API WINAPI GetQuote(
    int nClientID,
    const char* pszZqdm,
    char* pszResult,
    char* pszErrInfo)
{
    if (!IsConnectOK(nClientID))
    {
        sprintf(pszErrInfo, "GetQuote not connect for %d\n", nClientID);
        return;
    }

    BQTRADEX_SCOPE_LOCK;

    if (pszResult) {
        pszResult[0] = 0;
    }
    if (pszErrInfo) {
        pszErrInfo[0] = 0;
    }

    int  lLength = 0;
    char lBuffer[4090] = "";
    strcpy(lBuffer, "时间\t开盘价\t收盘价\t最高价\t最低价\t成交量\t成交额\n");
    lLength = (int)strlen(lBuffer);

    TradeXQuote* lpQuote = LoadQuote(pszZqdm);
    if (lpQuote)
    {
        lLength += sprintf(lBuffer + lLength, "%s\t%.3f\t%.3f\t%.3f\t%.3f\t%lld\t%.2lf\n",
            lpQuote->m_Time, lpQuote->m_OpenPrice, lpQuote->m_ClosePrice, lpQuote->m_HighPrice, lpQuote->m_LowPrice, lpQuote->m_Volume, lpQuote->m_Turnover);
    }

    lLength = (int)strlen(lBuffer);
    strncpy(pszResult, lBuffer, lLength);
    pszResult[lLength] = 0;
}

void BQTRADEX_API WINAPI GetTradableQuantity(
    int nClientID,
    int nCategory,
    int nPriceType,
    const char *pszGddm,
    const char *pszZqdm,
    float fPrice,
    char *pszResult,
    char *pszErrInfo)
{
    strcpy(pszErrInfo, "GetTradableQuantity not impl\n");
}

void BQTRADEX_API WINAPI Repay(
    int nClientID,
    const char* pszAmount,
    char* pszResult,
    char* pszErrInfo)
{
    strcpy(pszErrInfo, "Repay not impl");
}

//
//
void BQTRADEX_API WINAPI QueryHistoryData(
    int nClientID,
    int nCategory,
    const char* pszStartDate,
    const char* pszEndDate,
    char* pszResult,
    char* pszErrInfo)
{
    strcpy(pszErrInfo, "QueryHistoryData not impl\n");
}

void BQTRADEX_API WINAPI QueryDatas(
    int nClientID,
    int nCategory[],
    int nCount,
    char* pszResult[],
    char* pszErrInfo[])
{
    for (int i = 0; i < nCount; ++i)
    {
        QueryData(nClientID, nCategory[i], pszResult[i], pszErrInfo[i]);
    }
}

void BQTRADEX_API WINAPI SendOrders(
    int nClientID,
    int nCategory[],
    int nPriceType[],
    const char* pszGddm[],
    const char* pszZqdm[],
    float fPrice[],
    int nQuantity[],
    int nCount,
    char* pszResult[],
    char* pszErrInfo[])
{
    for (int i = 0; i < nCount; ++i)
    {
        SendOrder(nClientID, nCategory[i], nPriceType[i], pszGddm[i], pszZqdm[i], fPrice[i], nQuantity[i], pszResult[i], pszErrInfo[i]);
    }
}

void BQTRADEX_API WINAPI CancelOrders(
    int nClientID,
    const char* pszExchangeID[],
    const char* pszhth[],
    int nCount,
    char* pszResult[],
    char* pszErrInfo[])
{
    for (int i = 0; i < nCount; ++i)
    {
        CancelOrder(nClientID, pszExchangeID[i], pszhth[i], pszResult[i], pszErrInfo[i]);
    }
}

void BQTRADEX_API WINAPI GetQuotes(
    int nClientID,
    const char* pszZqdm[],
    int nCount,
    char* pszResult[],
    char* pszErrInfo[])
{
    for (int i = 0; i < nCount; ++i)
    {
        GetQuote(nClientID, pszZqdm[i], pszResult[i], pszErrInfo[i]);
    }
}

//
//
//

int BQTRADEX_API WINAPI QuickIPO(int nClientID)
{
    return -1;
}

int BQTRADEX_API WINAPI QuickIPODetail(
    int nClientID,
    int nCount,
    char* pszResultOK[],
    char* pszResultFail[],
    char* pszErrInfo)
{
    strcpy(pszErrInfo, "QuickIPODetail not impl");
    return -1;
}

//
// reverse repos
//
int BQTRADEX_API WINAPI ReverseRepos(int nClientID)
{
    return -1;
}


//////////////////////////////////////////////////////////////////////////
static void LogDebug(int level, const char* fmt, ...)
{
    (void)level;
    char lBuffer[4000] = "[TradeX] ";
    int  lLength = (int)strlen(lBuffer);

    time_t lNow = time(NULL);
    lLength += ConvertTime(lBuffer + lLength, lNow);
    lBuffer[lLength++] = ' ';

    va_list args;
    va_start(args, fmt);
    lLength += vsnprintf(lBuffer + lLength, sizeof(lBuffer) - lLength, fmt, args);
    va_end(args);

    lBuffer[lLength] = '\0';

    OutputDebugStringA(lBuffer);

    if (_out2stdout)
        printf(lBuffer);
}

static TradeXQuote* LoadQuote(const char* apZqdm)
{
    if (g_Quotes.empty())
    {
        TradeXQuote lQuote;

        // prepare manually or load from file
        memset(&lQuote, 0, sizeof(lQuote));
        strcpy(lQuote.m_Stock, "000001");
        strcpy(lQuote.m_StockName, "平安银行");
        lQuote.m_OpenPrice = 10.05f;
        lQuote.m_ClosePrice = 9.82f;
        lQuote.m_HighPrice = 10.15f;
        lQuote.m_LowPrice = 9.87f;
        lQuote.m_Volume = 156587447;
        lQuote.m_Turnover = 1565757000.0;
        strcpy(lQuote.m_Time, "2018-06-20 13:31:00");
        g_Quotes.insert(std::make_pair(string("000001"), lQuote));

        memset(&lQuote, 0, sizeof(lQuote));
        strcpy(lQuote.m_Stock, "600569");
        strcpy(lQuote.m_StockName, "安阳钢铁");
        lQuote.m_OpenPrice = 4.21f;
        lQuote.m_ClosePrice = 4.13f;
        lQuote.m_HighPrice = 4.33f;
        lQuote.m_LowPrice = 3.85f;
        lQuote.m_Volume = 123106282;
        lQuote.m_Turnover = 505840300.0;
        strcpy(lQuote.m_Time, "2018-06-20 13:31:20");
        g_Quotes.insert(std::make_pair(string("600569"), lQuote));
    }

    if (apZqdm == NULL)
    {
        apZqdm = "";
    }

    std::string lZqdm(apZqdm);
    if (g_Quotes.count(lZqdm))
    {
        return &g_Quotes[lZqdm];
    }
    return NULL;
}

static int ConvertTime(char aTimeBuf[], time_t aTime)
{
    struct tm* lptm = localtime(&aTime);
    return sprintf(aTimeBuf, "%04d-%02d-%02d %02d:%02d:%02d",
        lptm->tm_year + 1900, lptm->tm_mon + 1, lptm->tm_mday,
        lptm->tm_hour, lptm->tm_min, lptm->tm_sec);
}

static const char* GetStatusDesc(int aStatus)
{
    switch (aStatus)
    {
    case ORDER_STATUS_Inserted:     return "正报";
    case ORDER_STATUS_Accepted:     return "已报";
    case ORDER_STATUS_Traded:       return "已成";
    case ORDER_STATUS_PartTraded:   return "部成";
    case ORDER_STATUS_PartCancel:   return "部撤";
    case ORDER_STATUS_Canceled:     return "已撤";
    case ORDER_STATUS_Rejected:     return "废单";
    default:
        break;
    }
    return "未知";
}

static const char* GetDirectionDesc(int aDirection)
{
#if 1
    switch (aDirection)
    {
    case 0:     return "0";
    case 1:     return "1";
    case 2:     return "2";
    case 3:     return "3";
    default:
        break;
    }
    return " ";
#else
    switch (aDirection)
    {
    case 0:     return "买入";
    case 1:     return "卖出";
    case 2:     return "融资买入";
    case 3:     return "融券卖出";
    default:
        break;
    }
    return "其它";
#endif
}

static const char* GetPriceTypeDesc(int aPriceType)
{
    switch (aPriceType)
    {
    case 0:     return "限价";
    case 1:     return "市价对方最优";
    case 2:     return "市价本方最优";
    case 3:     return "即时成交剩余撤销";
    case 4:     return "五档即成剩撤";
    case 5:     return "全额成交或撤销";
    case 6:     return "五档即成转限价";
    default:
        break;
    }
    return "其它";
}

static const char* GetAccountNo(int32_t aClientId)
{
    ClientsMap::iterator iter = g_clients.begin();
    while (iter != g_clients.end())
    {
        ClientManager* lpClient = iter->second;
        if (lpClient->ClientID() == aClientId) {
            return lpClient->AccountNo();
        }
        ++iter;
    }
    return NULL;
}


static ClientManager* GetClientByAccntNo(const char* apAccountNo)
{
    ClientsMap::iterator iter = g_clients.begin();
    while (iter != g_clients.end())
    {
        ClientManager* lpClient = iter->second;
        
        if (strcmp(lpClient->AccountNo(), apAccountNo) == 0) {
            return lpClient;
        }
        ++iter;
    }
    return NULL;
}

static int32_t NextClientId()
{
    return g_ClientId++;
}

static int32_t NextOrderId()
{
    return g_OrderId++;
}

static int32_t NextMatchId()
{
    return g_MatchId++;
}


//////////////////////////////////////////////////////////////////////////
static void _UpdateInfoWhenMatched(double lastprice, TradeXOrderInfo* apOrder, TradeXPosition* apPosition, TradeXAccount* apAccount)
{
    LogDebug(0, "**Matched account:%s, code:%s, price:%.3lf, vol:%d, direction:%d\n", apAccount->m_AccountNo, 
        apOrder->m_Stock, lastprice, apOrder->m_Quantity, apOrder->m_Direction);

    // update order fields
    apOrder->m_Status = ORDER_STATUS_Traded;
    apOrder->m_Filled = apOrder->m_Quantity;

    apOrder->m_TradeTime = time(0);
    apOrder->m_TradeId = NextMatchId();
    apOrder->m_TradePrice = (float)lastprice;

    // update positoin
    StockPoistionUpdate(apPosition, apOrder->m_Direction, apOrder->m_Quantity, lastprice, true);

    // update trading account info
    double transaction_money = apOrder->m_TradePrice * apOrder->m_Quantity;
    if (apOrder->m_Direction == ORDER_DIRECTION_Buy)
    {
        apAccount->m_UsableMoney -= transaction_money;
        apAccount->m_PositionValue += transaction_money;
        apAccount->m_BalanceMoney -= transaction_money;
    }
    else
    {
        apAccount->m_UsableMoney += transaction_money;
        apAccount->m_PositionValue -= transaction_money;
        apAccount->m_BalanceMoney += transaction_money;
    }
}

static void _MatchOrder(const char* apStockCode, double lastprice, int lastvol, TradeXOrderInfo* apOrder, ClientManager* apClient)
{
    std::string lStockCode(apStockCode);
    TradeXAccount* lpAccount = apClient->GetAccount();

    if (strcmp(apStockCode, apOrder->m_Stock) != 0) {
        return;
    }
    if (apOrder->m_PriceType > 0)
    {
        if (lastprice < 1.0)
            lastprice = apOrder->m_Price;

        // market order
        TradeXPosition* lpPosition = apClient->GetPosition(lStockCode, true);
        _UpdateInfoWhenMatched(lastprice, apOrder, lpPosition, lpAccount);
        return;
    }
    if (lastprice < 1.0) {
        return;
    }

    TradeXPosition* lpPosition = apClient->GetPosition(lStockCode, true);

    bool lMatched = false;
    if (apOrder->m_Direction == ORDER_DIRECTION_Buy && apOrder->m_Price > lastprice)
    {
        lMatched = true;
    }
    else if (apOrder->m_Direction == ORDER_DIRECTION_Sell && apOrder->m_Price < lastprice)
    {
        lMatched = true;
    }
    else
    {
        // cannot match
        return;
    }

    _UpdateInfoWhenMatched(lastprice, apOrder, lpPosition, lpAccount);
}

void BQTRADEX_API WINAPI UpdateQuote(const TradeXQuote* apQuote)
{
    std::string lStockCode(apQuote->m_Stock);
    if (g_Quotes.count(lStockCode))
    {
        TradeXQuote* lpQuote = &g_Quotes[lStockCode];
        *lpQuote = *apQuote;
    }
    else
    {
        g_Quotes.insert(std::make_pair(lStockCode, *apQuote));
    }

    // match orders
    ClientsMap::iterator iter = g_clients.begin();
    while (iter != g_clients.end())
    {
        int32_t lClientId = iter->first;
        ClientManager* lpClient = iter->second;
        OrderContainer& lOrders = lpClient->Orders();
        OrderContainer::iterator ord_iter = lOrders.begin();
        for (; ord_iter != lOrders.end(); ++ord_iter)
        {
            TradeXOrderInfo* lpOrder = &(*ord_iter);
            _MatchOrder(apQuote->m_Stock, apQuote->m_LastPrice, (int)apQuote->m_Volume, lpOrder, lpClient);
        }
    }
}
