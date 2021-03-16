#include "backtest.h"
#include "model1.h"
#include "model2.h"
#include "model3.h"
#include <bits/stdc++.h>

void run_analysis() {
	Model1 model;
	for (Decimal makingDist = 0.1; makingDist <= 0.6; makingDist += 0.03)
		for (Decimal minVolatility = 0.0002; minVolatility <= 0.0006; minVolatility += 0.00003)
			for (int CACHE_SIZE = 100; CACHE_SIZE <= 100; CACHE_SIZE += 30) {
				Result r = model.run(makingDist, CACHE_SIZE, minVolatility);
				std::cerr << "markingDist: " << makingDist << ", minVolatility: " << minVolatility << " | " << " profit/fee: " << r.profit / r.fee << std::endl;
			}
}