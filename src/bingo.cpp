#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <algorithm>
#include <bitset>
#include <thread>
#include <stop_token>
#include <mutex>

#include <boost/dynamic_bitset.hpp>

#include <y9inc/random.h>
#include <y9inc/time.h>
#include <y9inc/string.h>
#include <CLI/CLI.hpp>


std::mutex mtx;
boost::dynamic_bitset<> thids;

struct Param {
	int size, maxNum;
	int threadNum, loopNum;
	std::pair<int,int> markPos;
};


// ビンゴカード 論理
struct Stat {
	uint64_t row = 0, column = 0, diagonal = 0, count = 0;
} g_stat;

struct Card {
	std::unordered_map<uint8_t, std::pair<int, int>> pos; // 数字→位置
	std::vector<std::vector<bool>> marked; // マーク済み状態
	uint8_t h, w;

	uint8_t type = 0;
	
	enum Type: uint8_t {
		row			= 1 << 0,
		column		= 1 << 1,
		diagonal	= 1 << 2,
	};

	Card(const std::vector<std::vector<uint8_t>>& grid) {
		h = grid.size();
		w = grid[0].size();
		marked.assign(h, std::vector<bool>(w, false));
		for (int i = 0; i < h; ++i)
			for (int j = 0; j < w; ++j)
				pos[grid[i][j]] = {i, j};
	}

	bool mark(uint8_t n) {
		auto it = pos.find(n);
		if (it == pos.end()) return false;
		auto [i, j] = it->second;
		marked[i][j] = true;
		return true;
	}

	void markPos(int x, int y) {
		marked[x][y] = true;
	}

	bool isBingo() {
		// 行
		for (int i = 0; i < h; ++i)
			if (std::all_of(marked[i].begin(), marked[i].end(), [](bool b){return b;})) {
				type |= Type::row;
			}
		// 列
		for (int j = 0; j < w; ++j) {
			bool ok = true;
			for (int i = 0; i < h; ++i)
				if (!marked[i][j]) ok = false;
			if (ok) {
				type |= Type::column;
			}
		}
		// 斜め
		bool diag1 = true, diag2 = true;
		for (int i = 0; i < h; ++i) {
			if (!marked[i][i]) diag1 = false;
			if (!marked[i][w - 1 - i]) diag2 = false;
		}
		if (diag1 || diag2) {
			type |= Type::diagonal;
		}

		return type;
	}
};

void calMain(std::stop_token st, const uint8_t& thid, const int& lpNum, const Param& param) {
	Stat stat;
	for (int i = 1; i <= lpNum; ++i) {
		
		std::vector<std::vector<uint8_t>> grid;
		grid.resize(param.size);
		std::bitset<256> exists;
		for (int h = 0; h < param.size; ++h) {
			grid[h].resize(param.size);
			// fill
			for (int w = 0; w < param.size; ++w) {
				uint8_t randNum;
				while (true) {
					randNum = randomNum(1,param.maxNum);
					if (exists[randNum]) continue;
					exists.set(randNum);
					break;
				}
				grid[h][w] = randNum;
			}
		}
		
		// ビンゴ試行
		Card card(grid);

		if (param.markPos != std::pair{-1,-1}) {
			card.markPos(param.markPos.first,param.markPos.second);
		}
		
		exists.reset();
		while (!card.isBingo() && !st.stop_requested()) {
			uint8_t randNum = randomNum(1,param.maxNum);
			if (exists[randNum]) continue;
			++stat.count;
			exists.set(randNum);
			card.mark(randNum);
		}
		
		if (card.type & Card::Type::row) stat.row++;
		if (card.type & Card::Type::column) stat.column++;
		if (card.type & Card::Type::diagonal) stat.diagonal++;
	}
	
	std::lock_guard<std::mutex> lock(mtx);
	g_stat.row += stat.row;
	g_stat.column += stat.column;
	g_stat.diagonal += stat.diagonal;
	g_stat.count += stat.count;
	thids.set(thid);
}


int main(int argc, char *argv[]) {
	CLI::App app;
	Param param;
	app.add_option("-s", param.size)->default_val(3);
	app.add_option("-n", param.maxNum)->default_val(75);
	app.add_option("-t", param.threadNum)->default_val(120);
	app.add_option("-l", param.loopNum)->default_val(100000);
	app.add_option("--mark", param.markPos)->default_val(std::array{-1,-1});

	CLI11_PARSE(app,argc,argv)
	if (param.size*param.size >= param.maxNum) throw std::runtime_error("Size^2 >= MaxNum");
	

	thids.resize(param.threadNum+1,false);
	
	int surplus = param.loopNum % param.threadNum;
	int npt = param.loopNum / param.threadNum;
	
	for (int i = 0; i < param.threadNum; ++i)
		new std::jthread(calMain,i,npt,param);
	new std::jthread(calMain,param.threadNum,surplus,param);
	
	size_t sec = 0;
	while (true) {
		if (thids.all()) break;
		sleepc(tu::s,1);
		++sec;
	}
	
	std::cout << sec << "秒\n\t横: " << std::to_string(g_stat.row) << "\n\t縦: " << std::to_string(g_stat.column) << "\n\t斜: " << std::to_string(g_stat.diagonal)
			  << "\n試行回数平均: " << std::to_string(static_cast<double>(g_stat.count) / param.loopNum) << "\n";
}
