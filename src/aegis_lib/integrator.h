#ifndef integrator__
#define integrator__

#include <iostream>
#include <cctype>
#include <vector>
#include <map>
#include <unordered_map>
#include <moab/Core.hpp>
#include "moab/Interface.hpp"
#include <moab/OrientedBoxTreeTool.hpp>
#include <set>
#include <variant>


using namespace moab;


using moab::OrientedBoxTreeTool;

struct comp
{
    template<typename T>
    bool operator()(const T &l, const T &r) const
    {
        if (l.second != r.second) {
            return l.second > r.second;
        }
        return l.first > r.first;
    }
};

typedef std::set<std::pair<EntityHandle, int>, comp> int_sorted_map;
typedef std::set<std::pair<EntityHandle, double>, comp> dbl_sorted_map;

// integrator .h file

class surfaceIntegrator 
{
  public:
  int raysTotal=0; // Total number of rays fired into geometry
  int raysHit=0; // Number of rays hit
  int nFacets=0; // Number of facets in geometry
  int raysLost;
  std::vector<EntityHandle> facetEntities; // list of all entity handles in geometry
  std::unordered_map<EntityHandle, int> nRays; // Number of rays intersected with a given surface EntityHandle
  std::unordered_map<EntityHandle, double> powFac; // power assigned to each facet
  
  // Methods
  surfaceIntegrator(moab::Range const &Facets); // constructor
  void count_hit(EntityHandle facet_hit); // count hits belonging to each facet

  void ray_reflect_dir(double const prev_dir[3], double const surface_normal[3], // get reflected dir
                         double reflected_dir[3]); 
  int_sorted_map sort_map(std::unordered_map<EntityHandle, int> const &map); // return sorted map of ints (as set<pair>)
  dbl_sorted_map sort_map(std::unordered_map<EntityHandle, double> const &map); // return sorted map of doubles (as set<pair)
};

#endif