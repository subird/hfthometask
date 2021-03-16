import pickle5 as pickle

trades_in = 'trades_eth_test.pkl'
trades_data = pickle.load(open(trades_in, 'rb'), encoding='latin1')
trades_out = open("trades.txt", "w")
for _, j in trades_data.iterrows():
	a = list(j)
	b = []
	b.append(a[0])
	b.append(a[4])
	b.append(a[5])
	b.append(int(a[6]))
	print(" ".join([str(x) for x in b]), file=trades_out)
trades_out.close()

book_in = 'orders_eth_depth50_test.pkl'
book_data = pickle.load(open(book_in, 'rb'), encoding='latin1')
book_out = open("book.txt", "w")
for _, j in book_data.iterrows():
	b = list(j)
	b[0] = int(b[0])
	print(" ".join([str(x) for x in b]), file=book_out)
book_out.close()