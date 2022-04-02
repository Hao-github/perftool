#include <fstream>
#include <iostream>
#include <string>

#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
int main(void){
  std::ofstream file("out.json");
	cereal::JSONOutputArchive archive(file);
	std::string s[] = { "this is a string", " 中文string也是可以支持的" };
	std::vector<double> vec = { 1.00001, 2e3, 30.1, -4, 5 };
	archive(CEREAL_NVP(vec), CEREAL_NVP(s));
}