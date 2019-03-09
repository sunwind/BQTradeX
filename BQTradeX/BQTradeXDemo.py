# -*- coding: utf-8 -*-

#import BQTradeX
import sys
reload(sys)
sys.setdefaultencoding('utf-8')

import ctypes
import pandas as pd

dir_dll = "./BQTradeX.dll"

class DllTrader():
    def __init__(self, broker, username, trdPwd, txPwd, moneyID):
        self.dll = ctypes.WinDLL(dir_dll)
        self.brokerId = broker
        self.ip = '127.0.0.1'
        self.port = 7708
        self.tdx_version = '0.1.0'
        self.username = username
        self.trdPwd = trdPwd
        self.txPwd = txPwd

        self.clientId = 0
        self.gddm_sh = ''
        self.gddm_sz = ''
        self.holder = None

        self._openTdx()
        self._logOn()

        gdInf = [i.split() for i in self._queryData(5).splitlines()]
        minLen = min([len(s) for s in gdInf])
        gdInf = [i[:minLen] for i in gdInf]
        print('gdInf:', gdInf)

        try:
            self.gddm_sh, self.gddm_sz = [str(i) for i in pd.DataFrame(gdInf[1:], columns = gdInf[0])[u'股东代码']][:2]
            print('SH:{0}, SZ:{1}'.format(self.gddm_sh, self.gddm_sz))
            if self.gddm_sz[0] == 'A':
                self.gddm_sh, self.gddm_sz = self.gddm_sz, self.gddm_sh
        except Exception as e:
            print('[error]DllTrader init exception:{0}'.format(e))
            self.gddm_sh, self.gddm_sz = [str(i) for i in pd.DataFrame(gdInf[1:], columns = gdInf[0])[u'股东号']][:2]

        #print(gdInf[1:])
        print(pd.DataFrame(gdInf[1:], columns = gdInf[0]))
        self.holder = pd.DataFrame(gdInf[1:], columns = gdInf[0])[u'股东名称'].iloc[0]
        print('holder:', self.holder)

    def _openTdx(self):
        return self.dll.OpenTdx()

    def _logOn(self):
        errInf = '\0' * 256
        self.clientId = self.dll.Logon(self.ip, self.port, self.tdx_version,
                             self.brokerId, self.username, self.username, self.trdPwd, self.txPwd, errInf)
        print('_logOn ', self.clientId)

    def _queryData(self, category = 0):
        '''
        查询各类交易数据
       Category：表示查询信息的种类，
           0资金  
           1股份   
           2当日委托  
           3当日成交     
           4可撤单   
           5股东代码  
           6融资余额   
           7融券余额  
           8可融证券   
        '''

        #print('query data count:', self.query_count)
        result = '\0'*1024*1024
        errInf = '\0'*256
        self.dll.QueryData(self.clientId, category, result, errInf)
        result = result.rstrip('\0')
        #print(errInf.decode("gbk")) 
        result_gbk = result.decode("gbk")
        return(result_gbk)

    def test_interface(self):
        clientId = 1

        # send orders category=0(buy), pricetype=0/1(limit/market order)
        results = '\0' * 1024
        errinfo = '\0' * 1024
        price = ctypes.c_float(12.05)
        self.dll.SendOrder(clientId, 0, 0, "001001001005793", "000001", price, 100, results, errinfo)
        results = results.rstrip('\0')
        errinfo= errinfo.rstrip('\0')
        print('SendOrder():\nresult:{0}\nerrinfo:{1}'.format(results, errinfo))

        results = '\0' * 1024
        errinfo = '\0' * 1024
        price = ctypes.c_float(11.20)
        self.dll.SendOrder(clientId, 0, 1, "001001001005793", "000001", price, 200, results, errinfo)
        results = results.rstrip('\0')
        errinfo= errinfo.rstrip('\0')
        print('SendOrder():\nresult:{0}\nerrinfo:{1}'.format(results, errinfo))

        # query order category=2
        category = 2
        results = '\0' * 1024
        errinfo = '\0' * 1024
        self.dll.QueryData(clientId, category, results, errinfo)
        results = results.rstrip('\0')
        errinfo= errinfo.rstrip('\0')
        print('QueryData(order):\nresult:{0}\nerrinfo:{1}'.format(results, errinfo))

        # query trades category=3
        category = 3
        results = '\0' * 1024
        errinfo = '\0' * 1024
        self.dll.QueryData(clientId, category, results, errinfo)
        results = results.rstrip('\0')
        errinfo= errinfo.rstrip('\0')
        print('QueryData(trade):\nresult:{0}\nerrinfo:{1}'.format(results, errinfo))

        # query pending orders category=4
        category = 4
        results = '\0' * 1024
        errinfo = '\0' * 1024
        self.dll.QueryData(clientId, category, results, errinfo)
        results = results.rstrip('\0')
        errinfo= errinfo.rstrip('\0')
        print('QueryData(pendingorder):\nresult:{0}\nerrinfo:{1}'.format(results, errinfo))

        # query quotes
        results = '\0' * 1024
        errinfo = '\0' * 1024
        self.dll.GetQuote(clientId, "000001", results, errinfo)
        results = results.rstrip('\0')
        errinfo= errinfo.rstrip('\0')
        print('GetQuote(000001):\nresult:{0}\nerrinfo:{1}'.format(results, errinfo))

        results = '\0' * 1024
        errinfo = '\0' * 1024
        self.dll.GetQuote(clientId, "600559", results, errinfo)
        results = results.rstrip('\0')
        errinfo= errinfo.rstrip('\0')
        print('GetQuote(600559):\nresult:{0}\nerrinfo:{1}'.format(results, errinfo))

        # query poistion
        self.poistion()

    def entrust(self, direction = 'all'):
        '''
        可撤委托
        ['StockCode', 'SotckName','Direction','EntruPrice','EntruVol','HitVol','EntruId','EntruTime']
        '''
        #从接口取数据
        clientId = 1
        results = '\0' * 1024
        errinfo = '\0' * 1024
        category = 4
        self.dll.QueryData(clientId, category, results, errinfo) 
        results = results.rstrip('\0').decode("gbk")

        res = results.splitlines()
        res = [i.split('\t') for i in res]
        entru = pd.DataFrame(res[1:], columns=res[0])

        #数据字段定义
        srcCols = [u'证券代码',u'证券名称',u'买卖标志',u'委托价格',u'委托数量',u'成交数量',u'委托编号',u'委托时间']
        objCols = ['StockCode', 'SotckName','Direction','EntruPrice','EntruVol','HitVol','EntruId','EntruTime']
        
        #不同券商字段名调整
        entru.columns = [u'证券代码',u'证券名称',u'买卖标志',u'委托类别',u'委托价格',u'委托数量',u'成交数量', u'委托编号',u'委托时间']
        entru = entru[srcCols].rename(columns = dict(zip(srcCols, objCols)))

        #数据格式变更
        for f in ['Direction', 'EntruVol','HitVol']:
            entru[f] = entru[f].astype(float).astype(int)
        entru['EntruPrice'] = entru['EntruPrice'].astype(float)

        if direction =='all':
            return(entru[entru['Direction'].isin([0,1])])
        if direction == 'buy':
            return(entru[entru['Direction'] == 0])
        if direction == 'sell':
            return(entru[entru['Direction'] == 1])

    def poistion(self):
        clientId = 1
        results = '\0' * 1024
        errinfo = '\0' * 1024
        category = 1
        self.dll.QueryData(clientId, category, results, errinfo) 
        results = results.rstrip('\0').decode("gbk")

        res = results.splitlines()
        res = [i.split('\t') for i in res]
        pos = pd.DataFrame(res[1:], columns=res[0])

        #数据字段定义
        srcCols = [u'证券代码', u'证券名称', 'Direction', u'今余额', u'可卖量', u'冻结数量', u'市值', u'买入金额']
        objCols = ['StockCode', 'SotckName','Direction','TotalSize','CoverableSize','FrozenSize','MarketValue','CostValue']

        #不同券商字段名调整
        pos['Direction'] = 0
        # here, just for xqzq
        pos.rename(columns = {u'证券数量':u'今余额', u'可卖数量':u'可卖量', u'最新市值':u'市值'}, inplace = True)
        
        entru = self.entrust(direction = 'sell')[['StockCode','EntruVol','HitVol']]
        entru['PendVol'] = entru['EntruVol'] - entru['HitVol']
        frozenSize = dict(entru.set_index('StockCode')['PendVol'])
        pos[u'冻结数量'] = pos[u'证券代码'].map(lambda x:frozenSize.get(x, 0))
        pos[u'买入金额'] = pos[u'成本价'].astype(float) * pos[u'今余额'].astype(float)

        pos = pos[srcCols].rename(columns = dict(zip(srcCols, objCols)))       
        #数据格式变更
        for f in ['TotalSize','CoverableSize','FrozenSize']:
            pos[f] = pos[f].astype(float).astype(int)
        for f in ['MarketValue','CostValue']:
            pos[f] = pos[f].astype(float)        
        print('pos:\n', pos)

if __name__ == "__main__":
    dlltrd = DllTrader(1, '1650002099', '123', '123', 0)
    print(dlltrd)

    dlltrd.test_interface()
    print('-' * 80);
