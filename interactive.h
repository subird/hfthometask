#include "backtest.h"
#include "model1.h"
#include "model2.h"
#include "model3.h"
#include <bits/stdc++.h>

void run_interactive() {
	Backtest ticker;
	ticker.init(MARKET_DELAY, 
				LIMIT_DELAY,
				CANCEL_DELAY, 
				FIRST_CURRENCY, 
				SECOND_CURRENCY, 
				TRADES_FILE, 
				ORDERBOOK_FILE, 
				LIMIT_FEE, 
				MARKET_FEE, 
				[&]() {},
	[](Decimal volume, const Backtest::Order &order, char type, Decimal current_fee, Timestamp current_timestamp) {
		if (type <= 1)
			std::cerr << "LIMIT ";
		else
			std::cerr << "MARKET ";
		if (type % 2 == 0)
			std::cerr << "BUY ";
		else
			std::cerr << "SELL ";
		std::cerr << "order with ID = " << order.id << " updated\n";
		if (!equal(volume, order.volume))
			std::cerr << "Partially executed " << volume << " ETH at price " << order.price << " USD\n" << order.sourceVolume - order.volume + volume << "/" << order.sourceVolume << " ETH already filled\n";
		else
			std::cerr << "Fully executed " << volume << " ETH at price " << order.price << " USD\n";
		std::cerr << "Volume: " << order.price * volume << " USD\n";
		std::cerr << "Fee: " << current_fee << " USD" << "\n";
		std::cerr << "Server time: " << current_timestamp << std::endl;
		std::cerr << std::endl;
	},
	[](const Backtest::Order &order, char type, Timestamp current_timestamp) {
		std::cerr << "Order received and placed\n";
		std::cerr << "ID: " << order.id << "\n";
		std::cerr << "Type: ";
		if (type <= 1)
			std::cerr << "LIMIT ";
		else
			std::cerr << "MARKET ";
		if (type % 2 == 0)
			std::cerr << "BUY\n";
		else
			std::cerr << "SELL\n";
		if (type <= 1)
			std::cerr << "Price: " << order.price << " USD\n";
		std::cerr << "Volume: " << order.volume << " ETH\n";
		std::cerr << "Server time: " << current_timestamp << "\n";
		std::cerr << std::endl;
	},
	[](const Backtest::Order &order, char type, Timestamp current_timestamp) {
		if (type == 5) {
			std::cerr << "Order with ID = " << order.id << " was requested to be canceled, but wasn't found\n";
			std::cerr << "Server time: " << current_timestamp << std::endl;
			std::cerr << std::endl;
			return;
		}
		std::cerr << "Order with ID = " << order.id << " successfully canceled\n";
		std::cerr << "Type: ";
		if (type <= 1)
			std::cerr << "LIMIT ";
		else
			std::cerr << "MARKET ";
		if (type % 2 == 0)
			std::cerr << "BUY\n";
		else
			std::cerr << "SELL\n";
		if (type <= 1)
			std::cerr << "Price: " << order.price << " USD\n";
		std::cerr << "Volume: " << order.volume << " ETH\n";
		std::cerr << "Server time: " << current_timestamp << "\n";
		std::cerr << std::endl;
	});
	std::function<void()> print_help = []() {
		std::cerr << "USAGE:\n\n";
		std::cerr << "market <buy|sell> <volume> -- submits market order\n";
		std::cerr << "limit <buy|sell> <volume> <price> -- submits limit order\n";
		std::cerr << "cancel <id> -- cancels order\n";
		std::cerr << "info -- shows total fee, balance, delays and time\n";
		std::cerr << "orderbook -- shows orderbook\n";
		std::cerr << "trades -- shows trades during last epoch\n";
		std::cerr << "next -- finish session until next epoch\n";
		std::cerr << "orders -- shows active orders\n";
		std::cerr << "pending -- shows pending requests\n";
		std::cerr << std::endl;
	};

	std::function<void()> print_info = [&]() {
		std::cerr << "INFO\n\n";
		std::cerr << "Total fee: " << ticker.getTotalFee() << "\n";
		std::cerr << "Balance: " << ticker.getBalance(0) << " USD" << ", " << ticker.getBalance(1) << " ETH\n";
		std::cerr << "Server time: " << ticker.getCurrentServerTime() << "\n";
		//std::cerr << "MARKET order fee: " << ticker.marketFee * ticker.EPOCH_SIZE << "%\n";
		//std::cerr << "LIMIT order fee: " << ticker.limitFee * ticker.EPOCH_SIZE << "%\n";
		//std::cerr << "MARKET order placement delay: " << ticker.marketDelay * ticker.EPOCH_SIZE << "ms\n";
		//std::cerr << "LIMIT order placement delay: " << ticker.limitDelay * ticker.EPOCH_SIZE << "ms\n";
		//std::cerr << "Cancel order delay: " << ticker.cancelDelay * ticker.EPOCH_SIZE << "ms \n";
		std::cerr << std::endl;
	};

	print_help();
	print_info();

	std::cerr << std::endl;
	//std::cerr << "> ";
	bool second = 0;
	do {
		if (second)
			std::cerr << "New epoch, server time is " << ticker.getCurrentServerTime() << std::endl;
		second = 1;
		while (1) {
			std::cerr << "> ";
			std::cerr.flush();
			std::string command;
			std::cin >> command;
			if (command == "info") {
				print_info();
			} else if (command == "trades") {
				std::cerr << "Trades over last " << ticker.EPOCH_SIZE << " ms\n\n";
				std::cerr << ticker.getLastTradesInfo();
				std::cerr << std::endl << std::endl;
			} else if (command == "next")
				break;
			else if (command == "market") {
				std::string type;
				Decimal volume;
				std::cin >> type >> volume;
				int id = ticker.submitMarket(volume, type == "buy");
				std::cerr << "Request submitted\n";
				std::cerr << "Request : order ID = " << id << ", MARKET " << (type == "buy" ? "BUY" : "SELL") << " " << volume << " ETH\n";
				std::cerr << "Server time: " << ticker.getCurrentServerTime() << "\n";
			} else if (command == "limit") {
				std::string type;
				Decimal volume, price;
				std::cin >> type >> volume >> price;
				int id = ticker.submitLimit(volume, type == "buy", price);
				std::cerr << "Request submitted\n";
				std::cerr << "Request : order ID = " << id << ", LIMIT " << (type == "buy" ? "BUY" : "SELL") << " " << volume << " ETH at price " << price << " USD\n";
				std::cerr << "Server time: " << ticker.getCurrentServerTime() << "\n";
				std::cerr << std::endl;
			} else if (command == "cancel") {
				int id;
				std::cin >> id;
				ticker.submitCancel(id);
				std::cerr << "Request submitted\n";
				std::cerr << "Cancel order with ID = " << id << "\n";
				std::cerr << "Server time: " << ticker.getCurrentServerTime() << "\n";
				std::cerr << std::endl;
			} else if (command == "orders") {
				std::cerr << "Active orders:\n\n";
				for (const Backtest::Order &order : ticker.getBuyLimits())
					std::cerr << "LIMIT BUY " << order.volume << " ETH at " << order.price << "\n";
				for (const Backtest::Order &order : ticker.getSellLimits())
					std::cerr << "LIMIT SELL " << order.volume << " ETH at " << order.price << "\n";
				for (const Backtest::Order &order : ticker.getMarketBuy())
					std::cerr << "MARKET BUY " << order.volume << " ETH\n";
				for (const Backtest::Order &order : ticker.getMarketSell())
					std::cerr << "MARKET SELL " << order.volume << " ETH\n";
			} else if (command == "pending") {
				std::cerr << "Pending requests:\n\n";
				for (const Backtest::limitRequest &r : ticker.getLimitPending())
					std::cerr << "ID = " << r.id << ", LIMIT " << (r.type % 2 == 0 ? "BUY " : "SELL ") << r.volume << " ETH at " << r.price << " USD ~" << r.time - ticker.getCurrentServerTime() << "ms left\n"; 
				for (const Backtest::marketRequest &r : ticker.getMarketPending())
					std::cerr << "ID = " << r.id << ", MARKET " << (r.type % 2 == 0 ? "BUY " : "SELL ") << r.volume << " ETH ~" << r.time - ticker.getCurrentServerTime() << "ms left\n";
				for (const Backtest::cancelRequest &r : ticker.getCancelPending())
					std::cerr << "CANCEL " << r.id << " ~" << r.time - ticker.getCurrentServerTime() << "ms left\n";
			} else if (command == "orderbook") {
				std::cerr << "BUY\n\n";
				const auto& currentBook = ticker.getCurrentBook();
				for (int i = 0; i < BOOK_DEPTH; ++i)
					std::cerr << currentBook.askVolume[i] << " ETH at " << currentBook.askPrice[i] << " USD\n";
				std::cerr << "\n---------\n\nSELL\n\n";
				for (int i = 0; i < BOOK_DEPTH; ++i)
					std::cerr << currentBook.bidVolume[i] << " ETH at " << currentBook.bidPrice[i] << " USD\n";
			} else if (command == "help") {
				print_help();
			} else
				std::cerr << "Invalid command, you may try 'help'\n";

		}
	} while (ticker.next());
}
