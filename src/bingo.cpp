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


std::mutex mtx;
boost::dynamic_bitset<> thids;

namespace param {
	uint8_t size(5), maxNum(75);
	int threadNum(120), loopNum(100000);
}

// ビンゴカード 論理
struct Stat {
	uint64_t row = 0, column = 0, diagonal = 0, count = 0;
} g_stat;

struct Card {
	std::unordered_map<uint8_t, std::pair<int, int>> pos; // 数字→位置
	std::vector<std::vector<bool>> marked; // マーク済み状態
	uint8_t h, w;
	
	enum class Type {
		none, row, column, diagonal
	} type;

	Card(const std::vector<std::vector<uint8_t>>& grid) {
		type = Type::none;
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

	bool isBingo() {
		// 行
		for (int i = 0; i < h; ++i)
			if (std::all_of(marked[i].begin(), marked[i].end(), [](bool b){return b;})) {
				type = Type::row;
				return true;
			}
		// 列
		for (int j = 0; j < w; ++j) {
			bool ok = true;
			for (int i = 0; i < h; ++i)
				if (!marked[i][j]) ok = false;
			if (ok) {
				type = Type::column;
				return true;
			}
		}
		// 斜め
		bool diag1 = true, diag2 = true;
		for (int i = 0; i < h; ++i) {
			if (!marked[i][i]) diag1 = false;
			if (!marked[i][w - 1 - i]) diag2 = false;
		}
		if (diag1 || diag2) {
			type = Type::diagonal;
			return true;
		}
		return false;
	}
};

void calMain(std::stop_token st, const uint8_t& thid, const int& lpNum) {
	Stat stat;
	for (int i = 1; i <= lpNum; ++i) {
		
		std::vector<std::vector<uint8_t>> grid;
		grid.resize(param::size);
		std::bitset<256> exists;
		for (int h = 0; h < param::size; ++h) {
			grid[h].resize(param::size);
			// fill
			for (int w = 0; w < param::size; ++w) {
				uint8_t randNum;
				while (true) {
					randNum = randomNum(1,param::maxNum);
					if (exists[randNum]) continue;
					exists.set(randNum);
					break;
				}
				grid[h][w] = randNum;
			}
		}
		
		// ビンゴ試行
		Card card(grid);
		exists.reset();
		while (!card.isBingo() && !st.stop_requested()) {
			uint8_t randNum = randomNum(1,param::maxNum);
			if (exists[randNum]) continue;
			++stat.count;
			exists.set(randNum);
			card.mark(randNum);
		}
		switch (card.type) {
			case Card::Type::row: ++stat.row; break;
			case Card::Type::column: ++stat.column; break;
			case Card::Type::diagonal: ++stat.diagonal; break;
		}
	}
	
	std::lock_guard<std::mutex> lock(mtx);
	g_stat.row += stat.row;
	g_stat.column += stat.column;
	g_stat.diagonal += stat.diagonal;
	g_stat.count += stat.count;
	thids.set(thid);
}

void parseCLIArg(const std::vector<std::string>& arg) {
	bool l1 = true;
	for (const std::string& p: arg) {
		if (l1) { // argv[0]を飛ばす
			l1 = false;
			continue;
		}
		if (!p.starts_with("-")) throw std::runtime_error("引数エラー [-]");
		switch (p[1]) {
			case 's': param::size = std::stoi(p.substr(2)); break;
			case 'n': param::maxNum = std::stoi(p.substr(2)); break;
			case 't': param::threadNum = std::stoi(p.substr(2)); break;
			case 'l': param::loopNum = std::stoi(p.substr(2)); break;
			default: throw std::runtime_error("引数エラー [" + std::string{p[1]} + "]");
		}
	}
	if (param::size*param::size >= param::maxNum) throw std::runtime_error("Size^2 >= MaxNum");
}

int main(int argc, char *argv[]) {
	parseCLIArg(st::charV(argc,argv));
	
	thids.resize(param::threadNum+1,false);
	
	int surplus = param::loopNum % param::threadNum;
	int npt = param::loopNum / param::threadNum;
	
	for (int i = 0; i < param::threadNum; ++i)
		new std::jthread(calMain,i,npt);
	new std::jthread(calMain,param::threadNum,surplus);
	
	size_t sec = 0;
	while (true) {
		if (thids.all()) break;
		sleepc(tu::s,1);
		++sec;
	}
	
	std::cout << sec << "秒\n\t横: " << std::to_string(g_stat.row) << "\n\t縦: " << std::to_string(g_stat.column) << "\n\t斜: " << std::to_string(g_stat.diagonal)
			  << "\n試行回数平均: " << std::to_string(static_cast<double>(g_stat.count) / param::loopNum) << "\n";
}
