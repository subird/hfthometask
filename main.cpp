#include <bits/stdc++.h>
#include "model3.h"

void run_best_model() {
	Model3 model;
	Result r = model.run();

	// learn() was partially to find coefficients
	
	//std::cout << "Profit: " << r.profit - r.fee << "\nFee: " << r.fee << "\nTotal submitted order count: " << r.allCnt << "\n";
}
signed main(int argc, char *argv[]) {
	std::ios_base::sync_with_stdio(false);

	if (argc != 7) {
		std::cout << "USAGE:\n\n ./run <CANCEL_DELAY> <LIMIT_DELAY> <MARKET_DELAY> <LIMIT_FEE> <MARKET_FEE> <INTERACTIVE/MODEL/ANALYSIS(0/1/2)>\n\n";
		return 0;
	}

	CANCEL_DELAY = atoi(argv[1]);
	LIMIT_DELAY = atoi(argv[2]);
	MARKET_DELAY = atoi(argv[3]);
	LIMIT_FEE = atof(argv[4]) / 100;
	MARKET_FEE = atof(argv[5]) / 100;
	int TYPE = atoi(argv[6]);

	if (TYPE == 1)
		run_best_model();
	/*else if (TYPE == 0) 
		run_interactive();
	else if (TYPE == 2) 
		run_analysis();*/
	return 0;
}