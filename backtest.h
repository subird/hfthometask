#include <bits/stdc++.h>
typedef long double Decimal;
typedef long long Timestamp;
const std::string TRADES_FILE = "trades.txt";
const std::string ORDERBOOK_FILE = "book.txt";
const int FIRST_CURRENCY = 0;
const int SECOND_CURRENCY = 1;
const bool LOWEST_PRIOR = 1;
const Timestamp EPOCH_SIZE = 100, BOOK_DEPTH = 50;
const Decimal priceEps = 1e-9, volumeEps = 1e-9;
const Decimal Inf = 1e20;
const Timestamp InfTime = 1e18;
bool sameEpoch(Timestamp a, Timestamp b) {
	return a / EPOCH_SIZE == b / EPOCH_SIZE;
}
bool equal(Decimal a, Decimal b) {
	return a - b < priceEps && a - b > -priceEps;
}
int sign(Decimal x) {
	return (x > 0 ? 1 : -1);
}
struct Result {
	Decimal profit, fee;
	int allCnt;
};
//delete abs;
int CANCEL_DELAY, LIMIT_DELAY, MARKET_DELAY;
Decimal LIMIT_FEE, MARKET_FEE;

struct Backtest {
public:
	struct Trade {
		Decimal volume, price;
		int type;
		Timestamp time;
	};
	struct marketRequest {
		Decimal volume;
		bool type;
		Timestamp time;
		int id;
	};
	struct cancelRequest {
		int id;
		Timestamp time;
	};
	struct limitRequest {
		Decimal volume, price;
		bool type;
		Timestamp time;
		int id;
	};
	struct Book {
		Decimal askPrice[50], bidPrice[50];
		Decimal askVolume[50], bidVolume[50];
		Timestamp time;
		void read(std::ifstream &bookRead, Timestamp &currentTimestamp) {
			bookRead >> time;
			currentTimestamp = time;
			for (int i = 0; i < BOOK_DEPTH; ++i)
				bookRead >> askPrice[i];
			for (int i = 0; i < BOOK_DEPTH; ++i)
				bookRead >> askVolume[i];
			for (int i = 0; i < BOOK_DEPTH; ++i)
				bookRead >> bidPrice[i];
			for (int i = 0; i < BOOK_DEPTH; ++i)
				bookRead >> bidVolume[i];
		}
	};
	struct Order {
		Decimal price, volume;
		int id;
		Decimal sourceVolume;
	};
	const Timestamp EPOCH_SIZE = 100;
public:
	Book currentBook;
	const int bookDepth = 50;

	int curr1, curr2;
	Decimal totalFee;
	int totalSubmittedOrdersCnt;
	Decimal limitFee, marketFee;
	int marketDelay = 1, limitDelay = 1, cancelDelay = 1;
	
	Decimal balance[2];
	int last_order_id;
	Timestamp currentTimestamp;
	Timestamp nextTradeTimestamp;
	
	std::ifstream tradesRead;
	std::ifstream bookRead;

	std::function<void(Decimal, Order const&, char, Decimal, Decimal)> orderChangedAlert;
	std::function<void(Order const&, char, Timestamp)> orderPlacedAlert, orderCanceledAlert;
	std::function<void()> newEpoch;

	std::vector<Order> buy_limits;
	std::vector<Order> sell_limits;
	std::deque<Order> market_buy;
	std::deque<Order> market_sell;
	
	
	// buy < sell

	std::deque<marketRequest> market_queue;
	std::deque<limitRequest> limit_queue;
	std::deque<cancelRequest> cancel_queue;

	int place_market(Decimal volume, bool buy, int id = -1) {
		if (id == -1)
			id = last_order_id++;
		if (buy) {
			market_buy.push_back({-Inf, volume, id, volume});
			orderPlacedAlert(market_buy.back(), 2, currentTimestamp);
		} else {
			market_sell.push_back({-Inf, volume, id, volume});
			orderPlacedAlert(market_sell.back(), 3, currentTimestamp);
		}
		return id;
	}

	bool cancel_order(int id) {
		auto equalId = [id](Order &limit) { return limit.id == id; };
		auto it_buy = std::find_if(buy_limits.begin(), buy_limits.end(), equalId);
		auto it_sell = std::find_if(sell_limits.begin(), sell_limits.end(), equalId);
		auto it_mbuy = std::find_if(market_buy.begin(), market_buy.end(), equalId);
		auto it_msell = std::find_if(market_sell.begin(), market_sell.end(), equalId);
		if (it_buy != buy_limits.end()) {
			orderCanceledAlert(*it_buy, 0, currentTimestamp);
			buy_limits.erase(it_buy);
			return 1;
		}
		if (it_sell != sell_limits.end()) {
			orderCanceledAlert(*it_sell, 1, currentTimestamp);
			sell_limits.erase(it_sell);
			return 1;
		}
		if (it_mbuy != market_buy.end()) {
			orderCanceledAlert(*it_mbuy, 2, currentTimestamp);
			market_buy.erase(it_mbuy);
			return 1;
		}
		if (it_msell != market_sell.end()) {
			orderCanceledAlert(*it_msell, 3, currentTimestamp);
			market_sell.erase(it_msell);
			return 1;
		}
		auto it_qmarket = std::find_if(market_queue.begin(), market_queue.end(), [id](marketRequest &r) { return r.id == id; });
		auto it_qlimit = std::find_if(limit_queue.begin(), limit_queue.end(), [id](limitRequest &r) { return r.id == id; });
		if (it_qmarket != market_queue.end()) {
			orderCanceledAlert(Order{-Inf, it_qmarket->volume, it_qmarket->id, it_qmarket->volume}, 2 + int(!(it_qmarket->type)), currentTimestamp);
			market_queue.erase(it_qmarket);
			return 1;
		}
		if (it_qlimit != limit_queue.end()) {
			orderCanceledAlert(Order{it_qlimit->price, it_qlimit->volume, it_qlimit->id, it_qlimit->volume}, int(!(it_qlimit->type)), currentTimestamp);
			limit_queue.erase(it_qlimit);
			return 1;
		}
		orderCanceledAlert({-Inf, -Inf, id, -Inf}, 5, currentTimestamp);
		return 0;
	}

	int place_limit(Decimal price, Decimal volume, bool buy, int id = -1) {
		if (id == -1)
			id = last_order_id++;
		if (buy) {
			buy_limits.push_back({price, volume, id, volume});
			orderPlacedAlert(buy_limits.back(), 0, currentTimestamp);
		} else {
			sell_limits.push_back({price, volume, id, volume});
			orderPlacedAlert(sell_limits.back(), 1, currentTimestamp);
		}
		return id;
	}
	std::vector<Trade> lastTrades;
	std::string tradesInfo;
	template<int type>
	void orderChanged(const Order &limit, Decimal volume) {
		if (type % 2 == 0)
			tradesInfo += "BUY_X ";
		else
			tradesInfo += "SELL_X ";
		tradesInfo += std::to_string(volume) + " ETH at " + std::to_string(limit.price) + " USD\n";
		lastTrades.push_back({volume, limit.price, type % 2, currentTimestamp - 1});
		Decimal current_fee;
		if (type <= 1)
			current_fee = limitFee * limit.price * volume;
		else
			current_fee = marketFee * limit.price * volume;
		totalFee += current_fee;
		if (type % 2 == 0)
			balance[curr2] += volume, balance[curr1] -= limit.price * volume;
		else
			balance[curr2] -= volume, balance[curr1] += limit.price * volume;
		orderChangedAlert(volume, limit, type, current_fee, currentTimestamp - 1);
	}
	Decimal askPrice, bidPrice;
	
public:
	void init(
		int _marketDelay, 
		int _limitDelay, 
		int _cancelDelay, 
		int _curr1, 
		int _curr2, 
		const std::string &tradesReadFile, 
		const std::string &bookReadFile, 
		Decimal _limitFee, 
		Decimal _marketFee, 
		std::function<void()> _newEpoch = 
						[](){}, 
		std::function<void(Decimal, Order const&, char, Decimal, Decimal)> _orderChangedAlert = 
						[](Decimal, Order const&, char, Decimal, Decimal) { return; }, 
		std::function<void(Order const&, char, Timestamp)> _orderPlacedAlert = 
						[](Order const&, char, Timestamp) {return;},
		std::function<void(Order const&, char, Timestamp)> _orderCanceledAlert = 
						[](Order const&, char, Timestamp) {return;}
	) {
		newEpoch = _newEpoch;
		marketDelay = _marketDelay;
		limitDelay = _limitDelay;
		cancelDelay = _cancelDelay;
		limitFee = _limitFee;
		marketFee = _marketFee;
		curr1 = _curr1;
		curr2 = _curr2;

		orderPlacedAlert = _orderPlacedAlert;
		orderChangedAlert = _orderChangedAlert;
		orderCanceledAlert = _orderCanceledAlert;
		
		last_order_id = 0;
		balance[0] = balance[1] = 0;
		totalFee = 0;
		totalSubmittedOrdersCnt = 0;

		tradesRead.clear();
		bookRead.clear();
		tradesRead.close();
		bookRead.close();
		tradesRead.open(tradesReadFile);
		bookRead.open(bookReadFile);
		
		currentBook.read(bookRead, currentTimestamp);
		askPrice = currentBook.askPrice[0];
		bidPrice = currentBook.bidPrice[0];
		tradesRead >> nextTradeTimestamp;

		buy_limits.clear();
		sell_limits.clear();
		market_buy.clear();
		market_sell.clear();
		market_queue.clear();
		limit_queue.clear();
		cancel_queue.clear();
	}


	Timestamp getCurrentServerTime() {
		return currentTimestamp;
	}
	Decimal getBalance(int curr) {
		return balance[curr];
	}
	Decimal getTotalFee() {
		return totalFee;
	}
	Decimal getCurrentAskPrice() {
		return askPrice;
	}
	Decimal getCurrentBidPrice() {
		return bidPrice;
	}
	const Book& getCurrentBook() {
		return currentBook;
	}
	const std::string& getLastTradesInfo() {
		return tradesInfo;
	}
	const std::vector<Trade>& getLastTrades() {
		return lastTrades;
	}
	const std::vector<Order>& getBuyLimits() {
		return buy_limits;
	}
	const std::vector<Order>& getSellLimits() {
		return sell_limits;
	}
	const std::deque<Order>& getMarketSell() {
		return market_sell;
	}
	const std::deque<Order>& getMarketBuy() {
		return market_buy;
	}
	const std::deque<limitRequest>& getLimitPending() {
		return limit_queue;
	}
	const std::deque<marketRequest>& getMarketPending() {
		return market_queue;
	}
	const std::deque<cancelRequest>& getCancelPending() {
		return cancel_queue;
	}

	bool ended() {
		return bookRead.eof();
	}
	bool next() {
		if (ended())
			return 0;
		sort(buy_limits.begin(), buy_limits.end(), [](Order &a, Order &b) {
			return a.price < b.price;
		});
		sort(sell_limits.begin(), sell_limits.end(), [](Order &a, Order &b) {
			return a.price > b.price;
		});
		while (market_queue.size() && market_queue.front().time == currentTimestamp) {
			place_market(market_queue.front().volume, market_queue.front().type, market_queue.front().id);
			market_queue.pop_front();
		}
		while (limit_queue.size() && limit_queue.front().time == currentTimestamp) {
			place_limit(limit_queue.front().price, limit_queue.front().volume, limit_queue.front().type, limit_queue.front().id);
			limit_queue.pop_front();
		}
		while (cancel_queue.size() && cancel_queue.front().time == currentTimestamp) {
			cancel_order(cancel_queue.front().id);
			cancel_queue.pop_front();
		}
		Decimal minBuy = Inf, minBuyVol;
		Decimal maxSell = -Inf, maxSellVol;
		lastTrades.clear();
		tradesInfo = "";
		// std::cout << nextTradeTimestamp << " " << currentTimestamp << std::endl;
		while (sameEpoch(currentTimestamp, nextTradeTimestamp)) {
			Decimal price, volume;
			bool isBuyerMaker;
			tradesRead >> price >> volume >> isBuyerMaker;
			//std::cout << nextTradeTimestamp << " " << price << " " << volume << " " << isBuyerMaker << "\n";
			if (isBuyerMaker)
				tradesInfo += "BUY ";
			else
				tradesInfo += "SELL ";
			lastTrades.push_back({volume, price, !isBuyerMaker, nextTradeTimestamp});
			tradesInfo += std::to_string(volume);
			tradesInfo += " ETH at ";
			tradesInfo += std::to_string(price);
			tradesInfo += " USD\n";
			if (isBuyerMaker) {
				if (equal(minBuy, price))
					minBuyVol += volume;
				else if (price < minBuy)
					minBuyVol = volume, minBuy = price;
			} else {
				if (equal(maxSell, price))
					maxSellVol += volume;
				else if (price > maxSell)
					maxSellVol = volume, maxSell = price;
			}
			if (!tradesRead.eof())
				tradesRead >> nextTradeTimestamp;
			else
				nextTradeTimestamp = InfTime;
		}
		Decimal minBuyVolBook = 0, maxSellVolBook = 0;
		int minBuyVolBookPos = -1, maxSellVolBookPos = -1;
		for (int i = 0; i < bookDepth; ++i) {
			Decimal price = currentBook.askPrice[i];
			if (!i)
				askPrice = price;
			if (minBuy > price)
				minBuy = price, minBuyVol = Inf;
			if (equal(maxSell, price))
				maxSellVolBookPos = i;
		}
		for (int i = 0; i < bookDepth; ++i) {
			Decimal volume = currentBook.askVolume[i];
			if (maxSellVolBookPos == i)
				maxSellVolBook = volume;
		}
		for (int i = 0; i < bookDepth; ++i) {
			Decimal price = currentBook.bidPrice[i];
			if (!i)
				bidPrice = price;
			if (maxSell < price)
				maxSell = price, maxSellVol = Inf;
			if (equal(minBuy, price))
				minBuyVolBookPos = i;
		}
		for (int i = 0; i < bookDepth; ++i) {
			Decimal volume = currentBook.bidVolume[i];
			if (minBuyVolBookPos == i)
				minBuyVolBook = volume;
		}
		while (buy_limits.size() && buy_limits.back().price > minBuy + priceEps) {
			orderChanged<0>(buy_limits.back(), buy_limits.back().volume);
			buy_limits.pop_back();
		}
		if (LOWEST_PRIOR)
			minBuyVol -= minBuyVolBook;
		minBuyVol = std::max(minBuyVol, (Decimal)0);
		while (buy_limits.size() && equal(buy_limits.back().price, minBuy) && minBuyVol > volumeEps) {
			Decimal tVol = std::min(minBuyVol, buy_limits.back().volume);
			orderChanged<0>(buy_limits.back(), tVol);
			buy_limits.back().volume -= tVol;
			minBuyVol -= tVol;
			if (buy_limits.back().volume < volumeEps)
				buy_limits.pop_back();
		}
		while (sell_limits.size() && sell_limits.back().price < maxSell - priceEps) {
			orderChanged<1>(sell_limits.back(), sell_limits.back().volume);
			sell_limits.pop_back();
		}
		if (LOWEST_PRIOR)
			maxSellVol -= maxSellVolBook;
		maxSellVol = std::max(maxSellVol, (Decimal)0);
		while (sell_limits.size() && equal(sell_limits.back().price, maxSell) && maxSellVol > volumeEps) {
			Decimal tVol = std::min(maxSellVol, sell_limits.back().volume);
			orderChanged<1>(sell_limits.back(), tVol);
			sell_limits.back().volume -= tVol;
			maxSellVol -= tVol;
			if (sell_limits.back().volume < volumeEps)
				sell_limits.pop_back();
		}
		while (market_buy.size()) {
			market_buy.front().price = askPrice;
			orderChanged<2>(market_buy.front(), market_buy.front().volume);
			market_buy.pop_front();
		}
		while (market_sell.size()) {
			market_sell.front().price = bidPrice;
			orderChanged<3>(market_sell.front(), market_sell.front().volume);
			market_sell.pop_front();
		}
		if (!ended()) {
			currentBook.read(bookRead, currentTimestamp);
			newEpoch();
		}
		return 1;
	}
	bool checkIfOrderModified(int id) {
		auto equalId = [id](Order &limit) { return limit.id == id; };
		auto it_buy = std::find_if(buy_limits.begin(), buy_limits.end(), equalId);
		auto it_sell = std::find_if(sell_limits.begin(), sell_limits.end(), equalId);
		auto it_mbuy = std::find_if(market_buy.begin(), market_buy.end(), equalId);
		auto it_msell = std::find_if(market_sell.begin(), market_sell.end(), equalId);
		/*
		if (it_buy != buy_limits.end()) 
			return it_buy->volume != it_buy->sourceVolume;
		if (it_sell != sell_limits.end()) 
			return it_sell->volume != it_sell->sourceVolume;
		if (it_mbuy != market_buy.end()) 
			return it_mbuy->volume != it_mbuy->sourceVolume;
		if (it_msell != market_sell.end()) 
			return it_msell->volume != it_msell->sourceVolume;
		*/
		if (it_buy != buy_limits.end()) 
			return 0;
		if (it_sell != sell_limits.end()) 
			return 0;
		if (it_mbuy != market_buy.end()) 
			return 0;
		if (it_msell != market_sell.end()) 
			return 0;

		auto it_qmarket = std::find_if(market_queue.begin(), market_queue.end(), [id](marketRequest &r) { return r.id == id; });
		auto it_qlimit = std::find_if(limit_queue.begin(), limit_queue.end(), [id](limitRequest &r) { return r.id == id; });
		if (it_qmarket != market_queue.end()) 
			return 0;
		if (it_qlimit != limit_queue.end()) 
			return 0;
		return 1;
	}
	int submitMarket(Decimal volume, bool type) {
		++totalSubmittedOrdersCnt;
		market_queue.push_back({volume, type, currentTimestamp + EPOCH_SIZE * marketDelay, last_order_id});
		return last_order_id++;
	}
	int submitLimit(Decimal volume, bool type, Decimal price) {
		++totalSubmittedOrdersCnt;
		limit_queue.push_back({volume, price, type, currentTimestamp + EPOCH_SIZE * limitDelay, last_order_id});
		return last_order_id++;
	}
	void submitCancel(int id) {
		cancel_queue.push_back({id, currentTimestamp + EPOCH_SIZE * cancelDelay});
	}
	int getTotalSubmittedOrdersCnt() {
		return totalSubmittedOrdersCnt;
	}
	Decimal PnL() {
		return getBalance(0) + getBalance(1) * getCurrentAskPrice() - getTotalFee();
	}
	void cancelAllOrders() {
		for (Order &order : buy_limits)
			submitCancel(order.id);
		for (Order &order : sell_limits)
			submitCancel(order.id);
		for (Order &order : market_buy)
			submitCancel(order.id);
		for (Order &order : market_sell)
			submitCancel(order.id);
	}
};