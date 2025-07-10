#ifndef CONFIG_H
#define CONFIG_H 1

#include<cstdint>

//#include "engines/toy.h"
//#include "engines/toy_tapered.h"
//#include "engines/fourku.h"
//#include "engines/fourkdotcpp.h"
//#include "engines/plantae.h"
#include "engines/weak.h"

//using TuneEval = Toy::ToyEval;
//using TuneEval = Toy::ToyEvalTapered;
//using TuneEval = Fourkdotcpp::FourkdotcppEval;
//using TuneEval = Plantae::PlantaeEval;
using TuneEval = Weak::WeakEval;

constexpr int32_t data_load_thread_count = 8;
constexpr int32_t thread_count = 8;
constexpr static bool print_data_entries = false;
constexpr static int32_t data_load_print_interval = 10000;


#endif // !CONFIG_H
