#pragma once
#include<vector>
#include<string>

namespace NarrativeWorldGeneration {

	__declspec(dllexport) std::string generateNarrativeWorldMold(const std::vector<std::string>& location_graph, const std::vector<std::string>& time_file);
	__declspec(dllexport) std::string generateNarrativeWorld(const std::string& mold_json, double time_point, int num_objects);
};