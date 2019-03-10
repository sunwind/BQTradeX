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



int BQTRADEX_API ReadConfig(const char* filename)
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

    fclose(fp);
    return 0;
}

