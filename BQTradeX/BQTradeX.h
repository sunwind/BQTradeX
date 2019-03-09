#ifndef _BQ_TRADE_X_H_
#define _BQ_TRADE_X_H_

#ifdef _MSC_VER

#ifdef BQTRADEX_EXPORTS
#define BQTRADEX_API __declspec(dllexport)
#else
#define BQTRADEX_API __declspec(dllimport)
#endif

#else /* unix platform */
#define BQTRADEX_API
#define WINAPI

#endif//_MSC_VER

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#define MAX_RESULT_SIZE   0x8010
#define MAX_ERRINFO_SIZE  1024

#ifdef __cplusplus
extern "C" {
#endif

void BQTRADEX_API WINAPI OpenTdx();

void BQTRADEX_API WINAPI CloseTdx();

int BQTRADEX_API WINAPI Logon(
    const char* pszIP,
    short nPort,
    const char* pszVersion,
    short nYybID,
    const char* pszAccountNo,
    const char* pszTradeAccount,
    const char* pszJyPassword,
    const char* pszTxPassword,
    char* pszErrInfo);

void BQTRADEX_API WINAPI Logoff(int nClientID);

bool BQTRADEX_API WINAPI IsConnectOK(int nClientID);

/* nCategory = 0: 查询资金
 * nCategory = 1: 查询股份
 * nCategory = 2: 查询当日委托
 * nCategory = 3: 查询当日成交
 * nCategory = 4: 查询可撤单
 * nCategory = 5: 查询股东代码
 */
void BQTRADEX_API WINAPI QueryData(
    int nClientID,
    int nCategory,
    char* pszResult,
    char* pszErrInfo);

/* 委托
 * SendOrder(0, 0, "p001001001005793", "601988", 0, 100)
 */
void BQTRADEX_API WINAPI SendOrder(
    int nClientID,
    int nCategory,
    int nPriceType,
    const char* pszGddm,
    const char* pszZqdm,
    float fPrice,
    int   nQuantity,
    char* pszResult,
    char* pszErrInfo);

void BQTRADEX_API WINAPI CancelOrder(
    int nClientID,
    const char* pszExchangeID,
    const char* pszhth,
    char* pszResult,
    char* pszErrInfo);

/* 查询五档行情
 * pszZqdm: ('000001', '600600')
 */
void BQTRADEX_API WINAPI GetQuote(
    int nClientID,
    const char* pszZqdm,
    char* pszResult,
    char* pszErrInfo);

/* 查询可交易股票数量
 */
void BQTRADEX_API WINAPI GetTradableQuantity(
    int nClientID,
    int nCategory,
    int nPriceType,
    const char *pszGddm,
    const char *pszZqdm,
    float fPrice,
    char *pszResult,
    char *pszErrInfo);

void BQTRADEX_API WINAPI Repay(
    int nClientID,
    const char* pszAmount,
    char* pszResult,
    char* pszErrInfo);

//
//
void BQTRADEX_API WINAPI QueryHistoryData(
    int nClientID,
    int nCategory,
    const char* pszStartDate,
    const char* pszEndDate,
    char* pszResult,
    char* pszErrInfo);

/* Category = (0, 1, 3) 查询资金、持仓
 */
void BQTRADEX_API WINAPI QueryDatas(
    int nClientID,
    int nCategory[],
    int nCount,
    char* pszResult[],
    char* pszErrInfo[]);

/* 批量委托
 * SendOrders(
 *           ((0, 0, "p001001001005793", "601988", 3.11, 100),
 *            (0, 0, "p001001001005793", "601988", 3.11, 200))
 * )
 */
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
    char* pszErrInfo[]);

/* 批量撤单
 */
void BQTRADEX_API WINAPI CancelOrders(
    int nClientID,
    const char* pszExchangeID[],
    const char* pszhth[],
    int nCount,
    char* pszResult[],
    char* pszErrInfo[]);

/* 批量查询行情
 */
void BQTRADEX_API WINAPI GetQuotes(
    int nClientID,
    const char* pszZqdm[],
    int nCount,
    char* pszResult[],
    char* pszErrInfo[]);

//
// below not impl yet
//

int BQTRADEX_API WINAPI QuickIPO(int nClientID);

int BQTRADEX_API WINAPI QuickIPODetail(
    int nClientID,
    int nCount,
    char* pszResultOK[],
    char* pszResultFail[],
    char* pszErrInfo);

//
// reverse repos
//
int BQTRADEX_API WINAPI ReverseRepos(int nClientID);


//
// this is our internal bqtradex interface
//
typedef struct stTradeXQuote TradeXQuote;
void BQTRADEX_API WINAPI UpdateQuote(const TradeXQuote* apQuote);



#ifdef __cplusplus
}
#endif


#endif _BQ_TRADE_X_H_
