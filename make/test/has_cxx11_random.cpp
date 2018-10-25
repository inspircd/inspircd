#include <iostream>
#include <random>

int main() {
	std::default_random_engine e{std::random_device{}()};
	std::cout << e();
}
