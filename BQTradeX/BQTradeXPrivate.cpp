#include <stdio.h>
#include <string>
#include "json.hpp"

#include "BQTradeX.h"
#include "BQTradeXPrivate.h"

using namespace std;
using json = nlohmann::json;


int StockPoistionUpdate(TradeXPosition* pos, int direction, int32_t volume, double price, bool istransaction)
{
    if (direction == ORDER_DIRECTION_Buy)
    {
        if (!istransaction)
        {
            return 0;
        }

        // + position
        int32_t lPriceSize = pos->m_TotalSize;
        pos->m_TotalSize += volume;
        pos->m_CoverableSize += volume;
        pos->m_CostBasis = ((pos->m_CostBasis * lPriceSize) + (volume * price)) / pos->m_TotalSize;
        pos->m_CostValue = pos->m_TotalSize * pos->m_CostBasis;
        pos->m_MarketValue = pos->m_TotalSize * price;
    }
    else if(direction == ORDER_DIRECTION_Sell)
    {
        if (!istransaction)
        {
            // foroze position
            if (volume > pos->m_CoverableSize)
            {
                return -1;
            }

            pos->m_FrozenSize += volume;
            pos->m_CoverableSize -= volume;
        }
        else
        {
            // - position
            int32_t lPriceSize = pos->m_TotalSize;
            pos->m_TotalSize -= volume;
            //pos->m_CoverableSize += volume;
            pos->m_FrozenSize -= volume;
            if (pos->m_TotalSize == 0)
                pos->m_CostBasis = 0.0;
            else
                pos->m_CostBasis = ((pos->m_CostBasis * lPriceSize) - (volume * price)) / pos->m_TotalSize;
            pos->m_CostValue = pos->m_TotalSize * pos->m_CostBasis;
            pos->m_MarketValue = pos->m_TotalSize * price;
        }
    }
    return 1;
}


int StockPositionUnfrozen(TradeXPosition* pos, int32_t volume)
{
    pos->m_CoverableSize += volume;
    pos->m_FrozenSize -= volume;
    return 1;
}



int BQTRADEX_API ReadConfig(const char* filename, double* apDefaultCash)
{
    char buf[8192] = "";
    FILE* fp = fopen(filename, "r");
    if (!fp)
    {
        fprintf(stderr, "open config file %s failed!\n", filename);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    int sz = (int)ftell(fp);

    fseek(fp, 0, SEEK_SET);
    int readlen = fread(buf, sz, 1, fp);

    auto j3 = json::parse(buf);

    double default_cash = j3["default_cash"];
    if (default_cash > 0 && apDefaultCash) {
        *apDefaultCash = default_cash;
    }

    json trading_account = j3["trading_account"];
    string accountID = trading_account["accountID"];
    double cash = trading_account["cash"];
    double cash_avail = trading_account["cash_avail"];

    json positions = j3["position"];
    sz = (int)positions.size();

    for (int i = 0; i < sz; ++i)
    {
        json pos = positions[i];
        string stock = pos["stock"];
        int currentQty = pos["currentQty"];
        double costPrice = pos["costPrice"];
        string stockName = pos["stockName"];
        int x = 0;
    }

    fclose(fp);
    return 0;
}

