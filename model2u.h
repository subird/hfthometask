
#include "backtest.h"
#include <bits/stdc++.h>

struct Model2u {
	Backtest ticker;
	Decimal nowPrice() {
		return (ticker.getCurrentAskPrice() + ticker.getCurrentBidPrice()) / 2;
	}
	std::deque<Decimal> lastPrices;
	std::deque<Decimal> pnl, dPrices;
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
	Decimal integrate(Decimal mean, Decimal std, Decimal x) {
		return (1 + erf((x - mean) / (sqrt(2) * std))) / 2;
	}
	std::valarray<Decimal> prob;
	void normalize() {
		Decimal sum = 0;
		for (Decimal x : prob)
			sum += x;
		for (Decimal &x : prob)
			x /= sum;
	}
	void normalize(std::valarray<Decimal> &prob) {
		Decimal sum = 0;
		for (Decimal x : prob)
			sum += x;
		for (Decimal &x : prob)
			x /= sum;
	}
	Decimal correctDirectionSignCoefficent, incorrectDirectionSignCoefficent, errorCoefficent, errorCoefficentTrade, priceStdDerv;
	std::valarray<Decimal> N(Decimal mean_price, Decimal std_derv_price) {
		std::valarray<Decimal> prob(cntPrices);
		for (int i = 0; i < cntPrices; ++i) {
			Decimal curr_price = getPrice(i);
			prob[i] = integrate(mean_price, std_derv_price, curr_price + 0.005) 
			        - integrate(mean_price, std_derv_price, curr_price - 0.005);
		}
		normalize(prob);
		return prob;
	}
	void initProb(Decimal mean_price) {
		cntPrices = int((maxPrice - minPrice + 1e-7) * 100);
		prob = N(mean_price, priceStdDerv);
		normalize(prob);
	}
	Decimal alpha, eta;
	void applyTrade(bool type, Decimal price) {
		int ind = index(price);
		if (type) {
			for (int i = 0; i < ind + 1; ++i) // market buy (taker)
				prob[i] *= incorrectDirectionSignCoefficent;
			for (int i = ind + 1; i < cntPrices; ++i)
				prob[i] *= correctDirectionSignCoefficent;
		} else {
			for (int i = 0; i < ind; ++i) // market sell (taker)
				prob[i] *= correctDirectionSignCoefficent;
			for (int i = ind; i < cntPrices; ++i)
				prob[i] *= incorrectDirectionSignCoefficent;
		}
		normalize();
		prob *= (1 - errorCoefficentTrade);
		prob += errorCoefficentTrade * N(nowPrice(), 1);
	   	normalize();
	}
	Decimal estPrice() {
		Decimal est = 0;
		for (int i = 0; i < cntPrices; ++i)
			est += prob[i] * getPrice(i);
		return est;
	}
	Decimal stdPrice() {
		Decimal e = estPrice(), sum = 0;
		for (int i = 0; i < cntPrices; ++i)
			sum += prob[i] * (getPrice(i) - e) * (getPrice(i) - e);
		return sqrt(sum);
	}
	Decimal noise(Decimal x) {
		return integrate(0, 0.001, x);
	} 
	Decimal rightPartB(Decimal pB) {
		int pBInd = index(pB);
		Decimal sum = 0;
		for (int i = 0; i < pBInd; ++i) 
			sum += ((1 - alpha) / 2 + alpha * noise(pB - getPrice(i))) * prob[i] * getPrice(i);
		for (int i = pBInd; i < cntPrices; ++i)
			sum += ((1 - alpha) / 2 + alpha * (1 - noise(getPrice(i) - pB))) * prob[i] * getPrice(i);
		return sum * 2;
	}
	Decimal rightPartA(Decimal pA) {
		int pAInd = index(pA);
		Decimal sum = 0;
		for (int i = 0; i < pAInd + 1; ++i) 
			sum += ((1 - alpha) / 2 + alpha * (1 - noise(pA - getPrice(i)))) * prob[i] * getPrice(i);
		for (int i = pAInd + 1; i < cntPrices; ++i)
			sum += ((1 - alpha) / 2 + alpha * noise(getPrice(i) - pA)) * prob[i] * getPrice(i);
		return sum * 2;
	}
	Decimal getPB() {
		int l = -1, r = cntPrices;
		while (r - l > 1) {
			int m = (l + r) / 2;
			if (rightPartB(getPrice(m)) > getPrice(m))
				r = m;
			else
				l = m;
		}
		return getPrice(l);
	}
	Decimal getPA() {
		int l = -1, r = cntPrices;
		while (r - l > 1) {
			int m = (l + r) / 2;
			if (rightPartA(getPrice(m)) < getPrice(m))
				r = m;
			else
				l = m;
		}
		return getPrice(r);
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
	Decimal dFQ = 0, maxFQ;
	Decimal getFQ() {
		return ticker.getBalance(1) + dFQ;
	}
	Decimal lastpA = 405, lastpB = 405;
	Timestamp lastClear = 0, lastUpdate = 0;
	void clearAll() {
		if (ticker.getCurrentServerTime() - lastClear < 500)
			return;

		lastClear = ticker.getCurrentServerTime();
		ticker.cancelAllOrders();
		if (getFQ() > volumeEps) {
			lastpA = lastpB = 400;
			ticker.submitMarket(std::abs(getFQ()), 0);
			dFQ -= getFQ();
		} else if (getFQ() < -volumeEps) {
			lastpA = lastpB = 400;
			ticker.submitMarket(std::abs(getFQ()), 1);
			dFQ -= getFQ();
		}
	}
	Decimal minabs(Decimal x, Decimal y) {
		if (x > y)
			return y;
		else if (x < -y)
			return -y;
		else
			return x;
	}
	Decimal sigmoid(Decimal x) {
		//return sign(x) * integrate(0.5, 0.1, abs(x)) * 0.02;
		return sign(x) * std::min(abs(x) * sqrt(abs(x)) * 0.07, (Decimal)0.015);
	}
	Decimal deltaCalc(Decimal x) {
		return integrate(0.001, 0.00023, x) * 0.03;
	}
	Result run() {
		lastPrices.clear();
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
					dPrices.push_back(abs(lastPrices.back() - nowPrice()));
					lastPrices.push_back(nowPrice());
					Decimal prevTradePrice = Inf;
					for (Backtest::Trade trade : ticker.getLastTrades()) 
						if (!equal(trade.price, prevTradePrice)) {
							applyTrade(trade.type, trade.price);
							prevTradePrice = trade.price;
						}
				},
				[&](Decimal volume, const Backtest::Order &order, char type, Decimal current_fee, Timestamp current_timestamp) {
					lastUpdate = ticker.getCurrentServerTime();
					if (type <= 1) {
						/*if (type % 2 == 0)
							waitQ -= volume;
						else
							waitQ += volume;*/
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
		
		alpha = 0.2;//, eta = 0.5;

		//incorrectDirectionSignCoefficent = (1 - alpha) - (1 - alpha) * eta;
		//correctDirectionSignCoefficent = (1 - alpha) * eta + alpha;

		incorrectDirectionSignCoefficent = 0.47;
		correctDirectionSignCoefficent = 0.53;
		maxFQ = 5;

		priceStdDerv = 0.117293212615426;
		errorCoefficent = 0.3;
		errorCoefficentTrade = errorCoefficent / 2;
		minPrice = 398;
		maxPrice = 412;
		
		initProb(nowPrice());
		for (Decimal i = 0; i < 1; i += 0.005)
			std::cerr << i << " " << sigmoid(i) << "\n";
		std::cerr << "=====\n";
		for (int i = index(nowPrice()) - 30; i <= index(nowPrice()) + 30; ++i)
			std::cerr << getPrice(i) << " " << rightPartA(getPrice(i)) << " "  << rightPartB(getPrice(i)) << " " << prob[i] << "\n";
		std::cerr << getPA() << " > " << getPB() << "\n";
		//exit(0);
		lastPrices.push_back(nowPrice());
		lastPrices.push_back(nowPrice());
		lastPrices.push_back(nowPrice());
		pnl.push_back(0);
		pnl.push_back(0);
		dPrices.push_back(0);
		
		Timestamp lastPlaced = 0;
		std::mt19937 rnd;
		int its = 0;
		while (!ticker.ended()) { 
			++its;
			prob *= (1 - errorCoefficent);
			prob += errorCoefficent * N(nowPrice(), priceStdDerv);
		   	normalize();

			Decimal ma5 = avg(lastPrices, 4);
			Decimal ma30 = avg(lastPrices, 50);
			Decimal jumpSize = ma5 - ma30;

			//Decimal dPrice = avg(dPrices, 10);

			Decimal pA = ticker.getCurrentAskPrice(), pB = ticker.getCurrentBidPrice();
			//pA = getPA(), pB = getPB();
			
			//pA = getPA();
			//pB = getPB();
			//Decimal d = dPrice + delt;

			//pA += d;
			//pB -= d;
			pA -= sigmoid(getFQ());
			pB -= sigmoid(getFQ());

			Decimal x = (avg(dPrices, 1000));
			Decimal delt = deltaCalc(x) - 0.001;

			pA += delt;
			pB -= delt;

			Decimal volt = x;

			//pA += x * deltaConst;
			//pB -= x * deltaConst;

			pA = getPrice(index(pA));
			pB = getPrice(index(pB));

			if ((ticker.getCurrentServerTime() - lastPlaced > volt * 10000 && 
				ticker.getCurrentServerTime() - lastClear > 5000) || ticker.getCurrentServerTime() - lastUpdate < 100) {
				ticker.cancelAllOrders();
				lastpA = pA;
				lastpB = pB;
				ticker.submitLimit(0.1, 1, pB);
				ticker.submitLimit(0.1, 0, pA);
				lastPlaced = ticker.getCurrentServerTime();
			}

			Decimal p1 = nowPrice();
			Decimal p2 = lastPrices[lastPrices.size() - 2];
			Decimal p3 = lastPrices[lastPrices.size() - 3];
			Decimal p4 = lastPrices[lastPrices.size() - 4];
			if (std::abs(p1 - p2) > volt * 7 ||
				(std::abs(p1 - p2) > volt * 4 && std::abs(p1 - p3) > volt * 4))
				clearAll();
			//if (ticker.getCurrentServerTime() - lastPlaced > 500 && (std::abs(jumpSize) > 0.01 || std::abs(nowPrice() - ma5) > 0.01)) 
			if (std::abs(p1 - p2) > 0.03 ||
				(std::abs(p1 - p2) > 0.012 && std::abs(p1 - p3) > 0.024) || 
				abs(getPA() - ticker.getCurrentAskPrice()) >= 0.03 ||
				abs(getPB() - ticker.getCurrentBidPrice()) >= 0.03)
				clearAll();

			std::cout
			<< ticker.getCurrentAskPrice() << " " 
			<< ticker.getCurrentBidPrice() << " " 
			<< estPrice() << " " 
			<< pA << " " 
			<< stdPrice() << " " 
			<< pB << " " 
			<< ticker.PnL() << " " 
			<< getFQ() << " " 
			<< volt << " " 
			<< ticker.getBalance(1) << " "
			<< (ticker.getTotalSubmittedOrdersCnt()) << " " 
			<< lastpA << " " 
			<< lastpB << " "
			<< delt
			<< std::endl;
			ticker.next();
			//if (its > 20000)
				//break;
		}
		return {ticker.getBalance(0) + ticker.getBalance(1) * ticker.getCurrentAskPrice(), ticker.getTotalFee(), ticker.getTotalSubmittedOrdersCnt()};
	}
};