#include "backtest.h"
#include <bits/stdc++.h>
// PnL = sum |price_i - price_{i - 1}| - E[(price_T - price_0) ^ 2]

struct Model3 {
	Backtest ticker;
	Decimal nowPrice() {
		return (ticker.getCurrentAskPrice() + ticker.getCurrentBidPrice()) / 2;
	}
	std::deque<Decimal> lastPrices;
	std::deque<Decimal> pnl, pnl_delt;
	Decimal priceMA(int x) {
		Decimal sum = 0;
		for (auto it = lastPrices.rbegin(); x && it != lastPrices.rend(); --x, ++it)
			sum += *it;
		sum /= x;
		return sum;
	}
	Decimal minPrice, maxPrice; 
	int cntPrices;
	int index(Decimal x) {
		return int((x - minPrice + 1e-7) * 100);
	}
	Decimal getPrice(int i) {
		return minPrice + (Decimal)i / 100;
	}
	Decimal avg(std::deque<Decimal> d, int cnt) {
		Decimal sum = 0;
		auto it = d.end();
		int xx = 0;
		while (it != d.begin() && cnt) {
			--it;
			sum += *it;
			--cnt;
			++xx;
		}
		return sum / xx;
	}
	std::vector<int> pos;
	Decimal dFQ = 0;
	Decimal getFQ() {
		return ticker.getBalance(1) + dFQ;
	}
	void clearAll() {
		ticker.cancelAllOrders();
		if (getFQ() > volumeEps) {
			ticker.submitMarket(std::abs(getFQ()), 0);
			dFQ -= getFQ();
		} else if (getFQ() < -volumeEps) {
			ticker.submitMarket(std::abs(getFQ()), 1);
			dFQ -= getFQ();
		}
	}
	void fixAll(Decimal price) {
		for (int i = 0; i < cntPrices; ++i) {
			if (i < index(price))
				pos[i] = ticker.submitLimit(0.01, 1, getPrice(i));
			else if (i > index(price))
				pos[i] = ticker.submitLimit(0.01, 0, getPrice(i));
		}
	}
	Decimal pnlScore() {
		return avg(pnl_delt, 10);
	}
	Result run() {
		lastPrices.clear();
		Decimal lastTradePrice = Inf;
		bool lastTradePriceType;
		Decimal minPriceFill, maxPriceFill;
		ticker.init(
				MARKET_DELAY, 
				LIMIT_DELAY,
				CANCEL_DELAY,  
				FIRST_CURRENCY, 
				SECOND_CURRENCY, 
				TRADES_FILE, 
				ORDERBOOK_FILE, 
				LIMIT_FEE, 
				MARKET_FEE, [&]() {
					lastPrices.push_back(nowPrice());
					lastTradePrice = Inf;

					for (Backtest::Trade trade : ticker.getLastTrades()) 
						lastTradePrice = trade.price, lastTradePriceType = trade.type;
					if (pnl.size())
						pnl_delt.push_back(ticker.PnL() - pnl.back());
					pnl.push_back(ticker.PnL());
				},
				[&](Decimal volume, const Backtest::Order &order, char type, Decimal current_fee, Timestamp current_timestamp) {
					if (type <= 1) {//std::cerr << "FILLED " << order.id << " " << order.price << "\n";
						std::cerr << "FILLED " << order.id << " " << order.price << "\n";
						minPriceFill = std::min(minPriceFill, order.price);
						maxPriceFill = std::max(maxPriceFill, order.price);
					} else {
						if (type % 2 == 0)
							dFQ -= volume;
						else
							dFQ += volume;
					}
				}, 
				[](Backtest::Order const&, char, Timestamp) {return;},
				[](Backtest::Order const&, char, Timestamp) {return;}
		);
		minPrice = 400;
		maxPrice = 411;
		cntPrices = (maxPrice - minPrice + 1e-7) * 100;
		pos.resize(cntPrices); 
		Decimal currPrice = ticker.getCurrentAskPrice();
		Decimal beginPrice = currPrice;
		fixAll(currPrice);
		std::mt19937 rnd;
		int waitIts = 0;
		bool risky = true;
		while (!ticker.ended()) {
			waitIts = std::max(0, waitIts - 1);
			//if (!waitIts) {
				/*if(!risky && (rnd() % 4000 == 0 || pnlScore() < -0.004)) {
					clearAll();
					waitIts = 100;
				}*/
			if (std::abs(minPriceFill) < 1e19 && std::abs(maxPriceFill) < 1e19) {
				int minInd = index(minPriceFill);
				int maxInd = index(maxPriceFill);
				int currInd = index(currPrice);
				int cntShort = std::max(0, currInd - minInd);
				int cntLong = std::max(0, maxInd - currInd);
				Decimal newPrice = currPrice + 0.01 * (cntLong - cntShort);
				std::cerr << minPriceFill << " " << maxPriceFill << " " << currPrice << " " << newPrice << " " << cntShort << " " << cntLong << "\n";
				currPrice = newPrice;
				int ind = index(newPrice);
				for (int i = ind - 200; i <= ind + 200; ++i) {
					if (i == ind)
						continue;
					bool ok = ticker.checkIfOrderModified(pos[i]);
					if (ok) {
						ticker.submitCancel(pos[i]);
						pos[i] = ticker.submitLimit(0.01, (i < ind), getPrice(i));
						std::cerr << "SUBMIT " << getPrice(i) << std::endl;
					}
				}
			}
			/*if (std::abs(currPrice - beginPrice - getFQ()) > 0.03) {
				dFQ -= currPrice - beginPrice;
				if (getFQ() > volumeEps) {
					ticker.submitMarket(std::abs(getFQ()), 0);
					dFQ -= getFQ();
				} else if (getFQ() < -volumeEps) {
					ticker.submitMarket(std::abs(getFQ()), 1);
					dFQ -= getFQ();
				}
				dFQ += currPrice - beginPrice; 
			}
			if (std::abs(currPrice - beginPrice - getFQ()) > 0.03)
				exit(1);*/
			std::cout << ticker.getCurrentAskPrice() << " " << ticker.getCurrentBidPrice() << " " << ticker.getTotalSubmittedOrdersCnt() << " " << ticker.PnL() << " " << getFQ() << " " << pnlScore() << " " << ticker.getBalance(1) << " " << 
			std::abs(currPrice - beginPrice - getFQ())<< std::endl;
			//}
			minPriceFill = Inf;
			maxPriceFill = -Inf;
			ticker.next();
		}
		return {ticker.getBalance(0) + ticker.getBalance(1) * ticker.getCurrentAskPrice(), ticker.getTotalFee(), ticker.getTotalSubmittedOrdersCnt()};
	}
};