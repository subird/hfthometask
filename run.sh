
cp trades_backup.txt trades.txt
cp book_backup.txt book.txt
g++-7 main.cpp -o run -std=c++17 -Ofast && ./run 0 0 0 -0.0005 0 1