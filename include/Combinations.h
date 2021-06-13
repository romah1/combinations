#pragma once

#include "Component.h"
#include "pugixml.hpp"

#include <filesystem>
//#include <pair>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct Component;

class Combinations
{
public:
    Combinations() = default;

    bool load(const std::filesystem::path & resource);

    std::string classify(const std::vector<Component> & components, std::vector<int> & order) const;

private:
    struct Leg
    {
        static Leg from_xml_leg(const pugi::xml_node & leg_node);
        static std::vector<Leg> from_xml_legs(const pugi::xml_node & legs_node);

        InstrumentType type{InstrumentType::Unknown};
        std::variant<double, char> ratio;
        std::variant<char, int> strike;
        std::variant<char, int, std::pair<int, char>> expiration;
    };

    struct Combination
    {
        static Combination from_xml_combination(const pugi::xml_node & combination_node);

        std::string name;
        std::string shortname;
        std::string identifier;
        std::string cardinality;
        unsigned long mincount{0};
        std::vector<Leg> legs;
    };

    static bool check_fits_fixed(const std::vector<Component> & components, const std::vector<Leg> & legs, std::vector<int> & order);
    static bool check_fits_fixed(std::vector<std::pair<Component, int>> & components, const std::vector<Leg> & legs, std::vector<int> & order);

    static bool check_fits_more(const std::vector<Component> & components, const Combination & combination, std::vector<int> & order);
    static bool check_fits_more(const std::vector<Component> & components, const Leg & leg);

    static bool check_fits_multiple(const std::vector<Component> & sorted_components, const std::vector<Leg> & legs, std::vector<int> & order);
    static bool check_fits_multiple(std::vector<std::pair<Component, int>> & sorted_components, const std::vector<Leg> & legs, std::vector<int> & order);

    static bool check_ratio_equivalence(const std::vector<std::pair<Component, Leg>> & match);
    static bool check_strike_order(std::vector<std::pair<Component, Leg>> & sorted_match);
    static bool check_expiration_order(std::vector<std::pair<Component, Leg>> & sorted_match);

    static bool check_all(std::vector<std::pair<Component, Leg>> & match);

    static std::vector<std::pair<Component, Leg>> pair_vectors(const std::vector<std::pair<Component, int>> & components, const std::vector<Leg> & legs, std::size_t from);
    static bool tm_equal(const std::tm & t1, const std::tm & t2);
    static bool tm_more(const std::tm & t1, const std::tm & t2);
    static bool is_option(const InstrumentType & i);
    static bool type_comparator(const InstrumentType & i1, const InstrumentType & i2);

    std::vector<Combination> combinations;
};
