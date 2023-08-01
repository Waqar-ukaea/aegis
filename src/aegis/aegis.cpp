#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>  // for setprecision
#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>  // for min/max values
#include <set>
#include <vector>
#include <array>
#include <algorithm>
#include <unordered_map>
#include <time.h>
#include <any>

#include <vtkCellArray.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkPolyLine.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkXMLMultiBlockDataWriter.h>
#include <vtkCompositeDataSet.h>
#include <vtkInformation.h>
#include <vtkSTLReader.h>
#include <vtkUnstructuredGrid.h>
#include <vtkUnstructuredGridReader.h>
#include <vtkUnstructuredGridWriter.h>
#include <vtkAppendFilter.h>

#include "DagMC.hpp"
#include "moab/Core.hpp"
#include "moab/Interface.hpp"
#include <moab/OrientedBoxTreeTool.hpp>
#include "settings.hpp"
#include "simpleLogger.h"
#include "equData.h"
#include "source.h"
#include "integrator.h"
#include "coordtfm.h"
#include "alglib/interpolation.h"
#include "vtkAegis.h"


using namespace moab;
using namespace coordTfm;

using moab::DagMC;
using moab::OrientedBoxTreeTool;
moab::DagMC* DAG;

void next_pt(double prev_pt[3], double origin[3], double next_surf_dist,
                          double dir[3], std::ofstream &ray_intersect);
double dot_product(double vector_a[], double vector_b[]);
double dot_product(std::vector<double> vector_a, std::vector<double> vector_b);
void reflect(double dir[3], double prev_dir[3], EntityHandle next_surf);
double * vecNorm(double vector[3]);


// LOG macros

//LOG_TRACE << "this is a trace message";             ***WRITTEN OUT TO LOGFILE***
//LOG_DEBUG << "this is a debug message";             ***WRITTEN OUT TO LOGFILE***
//LOG_WARNING << "this is a warning message";         ***WRITTEN OUT TO CONSOLE AND LOGFILE***
//LOG_ERROR << "this is an error message";            ***WRITTEN OUT TO CONSOLE AND LOGFILE***
//LOG_FATAL << "this is a fatal error message";       ***WRITTEN OUT TO CONSOLE AND LOGFILE***

int main() {




  clock_t start = clock();
  settings settings;
  settings.load_params();
  //settings.print_params();

 
  LOG_WARNING << "h5m Faceted Geometry file to be used = " << settings.sValues["DAGMC_input"];

  static const char* dagmc_input_file = settings.sValues["DAGMC_input"].c_str();
  static const char* vtk_input_file = settings.sValues["VTK_input"].c_str();

  static const char* ray_qry_exps = settings.sValues["ray_qry"].c_str();
  std::string eqdsk_file = settings.sValues["eqdsk_file"];
  moab::Range Surfs, Vols, Facets, Facet_vertices;
  DAG = new DagMC(); // New DAGMC instance
  DAG->load_file(dagmc_input_file); // open test dag file
  DAG->init_OBBTree(); // initialise OBBTree
  DAG->setup_geometry(Surfs, Vols);
  //DAG->create_graveyard();
  DAG->remove_graveyard();
  DAG->moab_instance()->get_entities_by_type(0, MBTRI, Facets);
  LOG_WARNING << "No of triangles in geometry " << Facets.size();

  //DAG->moab_instance()->get_entities_by_dimension()

  moab::EntityHandle triangle_set, vertex_set;
  DAG->moab_instance()->create_meshset( moab::MESHSET_SET, triangle_set );
  DAG->moab_instance()->add_entities(triangle_set, Facets);
  int num_facets;
  DAG->moab_instance()->get_number_entities_by_handle(triangle_set, num_facets);

  DAG->moab_instance()->get_entities_by_type(0, MBVERTEX, Facet_vertices);
  LOG_WARNING << "Number of vertices in geometry " << Facet_vertices.size();
  std::cout << std::endl;
  DAG->moab_instance()->create_meshset( moab::MESHSET_SET, vertex_set );
  DAG->moab_instance()->add_entities(vertex_set, Facet_vertices);
  DAG->moab_instance()->get_number_entities_by_dimension(vertex_set, 1 ,num_facets);
  moab::Range vertAdjs;

  moab::Range ents;
  DAG->moab_instance()->get_entities_by_handle(0, ents);
  for (auto i:Facets)
  {
    moab::Range verts;
    DAG->moab_instance()->get_adjacencies(&i, 1, 0, false, verts);
    //std::cout << "Tri " << DAG->moab_instance()->id_from_handle(i) << " vertex adjacencies:" << std::endl;
    std::vector<double> coords(3*verts.size());
    DAG->moab_instance()->get_coords(verts, &coords[0]);
    for (int j=0; j<verts.size(); j++)
    {
      //std::cout << "Vertex " << DAG->moab_instance()->id_from_handle(verts[j]) << "(" <<
      //coords[0+j] << ", " << coords[1+j] << ", "  << coords[2+j] << ")" << std::endl;
    }
      //std::cout << std::endl;

    // for (auto j:verts)
    // {
    //   double coords[3];
    //   DAG->moab_instance()->get_coords(&j, 3);
    //   std::cout << "Coords of each vertex" << coords[0] << " " << coords[1] << " " << coords[2] << std::endl;
    // }
  }



  //DAG->write_mesh("dag.out", 1);
  EntityHandle prev_surf; // previous surface id
  EntityHandle next_surf; // surface id
  double next_surf_dist=0.0; // distance to the next surface ray will intersect
  double prev_surf_dist; // previous next_surf_dist before the next ray_fire call
  DagMC::RayHistory history; // initialise RayHistory object
  EntityHandle vol_h = DAG->entity_by_index(3, 1);



  double dir_mag; // magnitude of ray direction vector
  double reflect_dot; // dot product of ray dir vector and normal vector
  double *normdir;
  int lost_rays=0;

  // --------------------------------------------------------------------------------------------------------
  if (settings.sValues["runcase"] =="rayqry") // rayqry test case
  {
  std::cout << "------------------------------------------------------" << std::endl;
  std::cout << "--------------------RAYQRY CASE-----------------------" << std::endl;
  std::cout << "------------------------------------------------------" << std::endl;

    std::vector<std::vector<double>> rayqry; // xyz data from rayqry file
    // Read in qry data
    std::ifstream ray_input(ray_qry_exps);
    double word;
    std::string line;
    if (ray_input) {
          while(getline(ray_input, line, '\n'))
          {
              //create a temporary vector that will contain all the columns
              std::vector<double> tempVec;
              std::istringstream ss(line);
              //read word by word(or int by int)
              while(ss >> word)
              {
                  //LOG_WARNING<<"word:"<<word;
                  //add the word to the temporary vector
                  tempVec.push_back(word);
              }
              //now all the words from the current line has been added to the temporary vector
              rayqry.emplace_back(tempVec);
          }
      }
      else
      {
          LOG_FATAL << "rayqry file cannot be opened";
      }
      ray_input.close();

      //now you can do the whatever processing you want on the vector
      int j=0;DAG->write_mesh("dag.out", 1);
      int k;
      int qrymax = rayqry.size();
      double dir_array[rayqry.size()][3];

      //lets check out the elements of the 2D vector so the we can confirm if it contains all the right elements(rows and columns)
      for(std::vector<double> &newvec: rayqry)
      {
        k=0;
          for(const double &elem: newvec)
          {
            dir_array[j][k]=elem;
            //LOG_TRACE << elem;
            k+=1;
          }
          j+=1;
      }

      double raydirs[qrymax][3];
      for (int j=0; j<qrymax; j+=2)
      {
        for (int k=0; k<3; k++)
        {
          raydirs[j][k] = dir_array[j+1][k] - dir_array[j][k];
        }
        //LOG_TRACE << raydirs[j][0] << ", " << raydirs[j][1] << ", " << raydirs[j][2]; // print out all ray directions from ray_qry

      }
      // define qrydata origin
      double qryorigin[3];
      double intersect[3];
      qryorigin[0] = dir_array[0][0];
      qryorigin[1] = dir_array[0][1];
      qryorigin[2] = dir_array[0][2];
      std::ofstream ray_intersect("ray_intersect.txt"); // stream for ray-surface intersection points
      ray_intersect << std::setprecision(5) << std::fixed;
      //ray_intersect << qryorigin[0] << ' ' << qryorigin[1] << ' ' << qryorigin[2] << ' ' ; // store first ray origin

      //loop through qrydata
      for (int qrycount=0; qrycount < qrymax; qrycount+=2){
        normdir = vecNorm(raydirs[qrycount]);
        // print normalised ray directions

        LOG_TRACE << normdir[0] << ", " << normdir[1] << ", " << normdir[2]; // print out all ray directions from ray_qry

        history.reset(); // reset history before launching ray
        // launch
        DAG->ray_fire(vol_h, qryorigin, normdir, next_surf, next_surf_dist, &history, 0, 1);
        // Count number of lost rays
        if (next_surf == 0)
        {
          lost_rays += 1;
        }

        // calculate next intersection point and write out to textfile
        for (int i=0; i<3; ++i)
        { // Calculate ray intersect point
          intersect[i] = qryorigin[i] + (next_surf_dist * normdir[i]);
          ray_intersect << intersect[i] << ' ';
        }
        ray_intersect << std::endl;
      }
      LOG_ERROR << "Lost ray count = " << lost_rays;
  }

  // --------------------------------------------------------------------------------------------------------
  else if (settings.sValues["runcase"] == "specific") // specific ray case
  {
    std::cout << "------------------------------------------------------" << std::endl;
    std::cout << "--------------------SOURCE DEFINED CASE---------------------" << std::endl;
    std::cout << "------------------------------------------------------" << std::endl;

    std::ofstream ray_coords1; // define stream ray_coords1
    std::ofstream ray_coords2; // define stream ray_coords2
    std::ofstream ray_intersect; // stream for ray-surface intersection points
    int reflection = 1;
    int nSample = 10000; // Number of rays sampled
    ray_coords1.open("ray_coords1.txt"); // write out stream ray_coords1 to "ray_coords1.txt" file
    ray_coords2.open("ray_coords2.txt"); // write out stream ray_coords2 to "ray_coords1.txt" file
    ray_intersect.open("ray_intersect.txt"); // write out stream to "ray_intersect.txt" file



    surfaceIntegrator integrator(Facets);
    //std::map<EntityHandle, int>::iterator it = integrator.nRays.begin();
    int lostRays=0;

    double intersect_pt[3];
    std::vector<double> pSource(3);
    pSource = {0, 0, -30};
    EntityHandle meshElement;
    double distanceMesh;
    pointSource spatialSource(pSource);
    double reflected_dir[3];
    double reflect_dot;
    double surface_normal[3];

    EntityHandle obb_root;
    //DAG->next_vol(Surfs[50], vol_h, vol_h);
    DAG->get_root(vol_h, obb_root);
    //DAG->obb_tree()->print(obb_root, std::cout, false);
    EntityHandle facet_hit;
    for (int i=0; i<nSample; i++)
    {
      spatialSource.get_isotropic_dir();
      history.reset();
      DAG->ray_fire(vol_h, spatialSource.r, spatialSource.dir, next_surf, next_surf_dist, &history, 0, 1);
      history.get_last_intersection(facet_hit);

      // std::cout << "obb_tree() " << DAG->obb_tree() << std::endl;
      // std::cout << "get_geom_tag() " << DAG->geom_tag() << std::endl;
      if (next_surf == 0)
      {
        next_surf_dist = 0;
        lostRays +=1;
      }
      else
      {
        for (int j=0; j<3; j++)
        {
          intersect_pt[j] = spatialSource.r[j] + next_surf_dist*spatialSource.dir[j];
        }
        integrator.count_hit(facet_hit);


        ray_intersect << spatialSource.r[0] << ' ' << spatialSource.r[1] << ' ' << spatialSource.r[2] << std::endl;
        ray_intersect << intersect_pt[0] << ' ' << intersect_pt[1] << ' ' << intersect_pt[2] << std::endl;

      ray_coords1 << spatialSource.dir[0] << ' ' << spatialSource.dir[1] << ' ' << spatialSource.dir[2] << std::endl;



      // REFLECTION CODE
        // if (reflection==1)
        // {
          // DAG->get_angle(next_surf, NULL, surface_normal, &history);
          // reflect_dot = dot_product(spatialSource.dir, surface_normal);

          // reflected_dir[0] = spatialSource.dir[0] - 2*reflect_dot*surface_normal[0];
          // reflected_dir[1] = spatialSource.dir[1] - 2*reflect_dot*surface_normal[1];
          // reflected_dir[2] = spatialSource.dir[2] - 2*reflect_dot*surface_normal[2];

          // DAG->ray_fire(vol_h, intersect_pt, reflected_dir, next_surf, next_surf_dist, &history, 0, 1);
          // history.get_last_intersection(facet_hit);
          //   if (reflected_dir != spatialSource.dir)
          //   {
          //     history.reset();
          //   }
          //   for (int j=0; j<3; j++)
          //   {
          //     intersect_pt[j] = intersect_pt[j] + next_surf_dist*reflected_dir[j];
          //   }
          //   DAG->closest_to_location(vol_h, intersect_pt, distanceMesh, &meshElement);
          //   integrator.count_hit(facet_hit);

          //   ray_intersect << intersect_pt[0] << ' ' << intersect_pt[1] << ' ' << intersect_pt[2] << std::endl;
          // //}
      }
    }


    LOG_WARNING << "Number of rays lost = " << lostRays;
    LOG_WARNING << "Number of rays launched = " << nSample;
    LOG_WARNING << "Number of ray-facet intersections = " << integrator.raysHit;

    integrator.facet_values(integrator.nRays);






  }

  // --------------------------------------------------------------------------------------------------------

  else if (settings.sValues["runcase"]=="eqdsk")
  {
    std::cout << "------------------------------------------------------" << std::endl;
    std::cout << "--------------------READING EQDSK---------------------" << std::endl;
    std::cout << "------------------------------------------------------" << std::endl;
    equData EquData;

    EquData.read_eqdsk(eqdsk_file);
    //EquData.write_eqdsk_out();

    EquData.init_interp_splines();
    EquData.gnuplot_out();
    EquData.centre(1);
    EquData.boundary_rb();

    // attempting to create a trace through magnetic field
    // Currently not working


    bool plotRZ = true;
    bool plotXYZ = true;
    EquData.write_bfield(plotRZ, plotXYZ);
    std::vector<double> cartPosSource(3);
    
    double normB[3];
    double norm;
    std::ofstream trace2("trace2.txt");
    std::ofstream trace3("trace3.txt");

    double newptA[3];
    double phi;
    std::vector<double> Bfield;
    std::vector<double> polarPos(3);
    std::vector<double> newPt(3);

    ////////// Particle tracking


    // Plasma facing surface of HCLL first wall structure
    moab::Range targetFacets; // range containing all of the triangles in the surface of interest

    // can specify particular surfaces of interest
    EntityHandle targetSurf; // surface of interest
    targetSurf = Surfs[0];
    if (settings.surf != 0)
    {

        targetSurf = DAG->entity_by_id(2, settings.surf); // front facing surface of HCLL
        DAG->moab_instance()->get_entities_by_type(targetSurf, MBTRI, targetFacets);

        LOG_WARNING << "Surface IDs provided. Launching from surfaces given by global IDs - " << std::endl << settings.surf;
      
    }
    else
    {
      targetFacets = Facets;
      LOG_WARNING << "No surface ID provided. LAunching from all surfaces by default";
    }

    //DAG->moab_instance()->list_entity(targetSurf); // list geometric information about surface
    surfaceIntegrator integrator(targetFacets);
    EntityHandle hit;

    std::vector<double> triA(3), triB(3), triC(3);
    std::vector<double> randTri;
    double fieldDir[3];
    double Bn; // B.n at surface of geometry
    

    double s; 
    
    double ds = settings.dValues["dsTrack"];
    int nS = settings.iValues["nTrack"];


    int iteration_count = 0;
    int trace_count = 0;
    double coords[9];
    int zSign;

    double psol = settings.dValues["Psol"];
    double lambda_q = settings.dValues["lambda_q"];



    // INITIALISE VTK STUFF -------------------------------------------------

    vtkAegis aegisVTK;

    vtkNew<vtkMultiBlockDataSet> multiBlockRoot, multiBlockBranch;
    std::map<std::string, vtkNew<vtkMultiBlockDataSet>> vtkParticleTracks;

    const char* branchShadowedPart = "Shadowed Particles";
    const char* branchLostPart = "Lost Particles";
    const char* branchDepositingPart = "Depositing Particles";
    const char* branchMaxLengthPart = "Max Length Particles";
    
    
    
    

    multiBlockRoot->SetBlock(0, multiBlockBranch); // set block 
    multiBlockRoot->GetMetaData(static_cast<int>(0)) // name block
                  ->Set(vtkCompositeDataSet::NAME(), "Particle Tracks");
    LOG_INFO << "Initialising particle_tracks root ";


    // Read in STL file 
    vtkNew<vtkSTLReader> vtkstlReader; // STL reader 
    vtkstlReader->SetFileName(vtk_input_file);
    vtkstlReader->Update();

    vtkNew<vtkUnstructuredGrid> vtkTargetUstr;

    // Transform PolyData to vtkUnstructuredGrid datatype using append filter
    vtkNew<vtkAppendFilter> appendFilter;
    vtkPolyData* vtkTargetPD = vtkstlReader->GetOutput(); 
    appendFilter->AddInputData(vtkTargetPD);
    appendFilter->Update();
    vtkTargetUstr->ShallowCopy(appendFilter->GetOutput());

    // set metadata associated with array(s)
    vtkNew<vtkDoubleArray> vtkTargetHeatflux;
    vtkTargetHeatflux->SetNumberOfComponents(1);
    vtkTargetHeatflux->SetName("Heat_Flux");
    LOG_INFO << "Initialising vtkUnstructuredGrid... ";


    int facetCounter=0;





    // LOOP OVER FACETS OF INTEREST ------------------------------------------------    

    std::string particleTrace = settings.sValues["trace"];

    for (auto i:targetFacets)
    {
      facetCounter +=1;

      //DAG->next_vol(targetSurf, vol_h, vol_h);
      iteration_count +=1;
      moab::Range HCLLverts;
      std::vector<EntityHandle> verts;
      DAG->moab_instance()->get_adjacencies(&i, 1, 0, false, verts);
      DAG->moab_instance()->get_coords(&verts[0], verts.size(), coords);

      for (int j=0; j<3; j++)
      {
        triA[j] = coords[j];
        triB[j] = coords[j+3];
        triC[j] = coords[j+6];
      }


      triSource Tri(triA, triB, triC);
      randTri = Tri.random_pt();
      integrator.launchPositions[i] = randTri;

      vtkNew<vtkPoints> vtkpoints;
      int vtkPointCounter = 0;


      double triStart[3];

      //std::cout << "Tri EntityHandle: " << i << " normal = " << Tri.normal << std::endl;

      if (particleTrace == "yes")
      {
        trace3 << randTri[0] << " " << randTri[1] << " " << randTri[2] << std::endl;
        vtkpoints->InsertNextPoint(randTri[0], randTri[1], randTri[2]);
        vtkPointCounter +=1;

      }
      triStart[0] = randTri[0];
      triStart[1] = randTri[1];
      triStart[2] = randTri[2];

      if (triStart[2] > 0)
      {
        zSign = 1;
      }
      else
      {
        zSign = -1;
      }

      Bfield = EquData.b_field(randTri, "cart");

      if (Bfield[0] == 0 && Bfield[1] == 0 && Bfield[2] == 0)
      {
        LOG_INFO << "Position of triangle start not in magnetic field. Skipping to next triangle";
        continue; // skip to next triangle if start position not in magnetic field
      }
      std::string forwards = "forwards";
      polarPos = coordTfm::cart_to_polar(randTri, forwards);
      Bfield = EquData.b_field_cart(Bfield, polarPos[2], 0);
      norm = sqrt(pow(Bfield[0],2) + pow(Bfield[1],2) + pow(Bfield[2],2));
      normB[0] = Bfield[0]/norm;
      normB[1] = Bfield[1]/norm;
      normB[2] = Bfield[2]/norm;

      Bn = dot_product(Bfield,Tri.normal);
      //std::cout << " NORMAL = " << Tri.normal << " B.N = " <<  Bn << std::endl;
      if (Bn < 0)
      {
        fieldDir[0] = -normB[0];
        fieldDir[1] = -normB[1];
        fieldDir[2] = -normB[2];
      }
      else if (Bn > 0)
      {
        fieldDir[0] = normB[0];
        fieldDir[1] = normB[1];
        fieldDir[2] = normB[2];
      }

      // DAG->ray_fire(vol_h, triStart, normB, next_surf, next_surf_dist, &history, ds, 1);
      // std::cout << Bfield << std::endl;

      DAG->ray_fire(vol_h, triStart, fieldDir, next_surf, next_surf_dist, &history, ds, 1);
      if (next_surf != 0) {DAG->next_vol(next_surf, vol_h, vol_h);}


      for (int j=0; j<3; ++j)
      {
        newPt[j] = triStart[j] + fieldDir[j]*ds;
      }

      if (particleTrace == "yes")
      {
        vtkpoints->InsertNextPoint(newPt[0], newPt[1], newPt[2]);
        vtkPointCounter +=1;
      }
      history.rollback_last_intersection();
      //history.reset();
      s = ds;
      bool traceEnded = false; 

// --------------------- LOOP OVER PARTICLE TRACK --------------------- 
      for (int j=0; j < nS; ++j) // loop along fieldline
      {
        traceEnded = false;
        s += ds;
        newptA[0] = newPt[0];
        newptA[1] = newPt[1];
        newptA[2] = newPt[2];
        DAG->ray_fire(vol_h, newptA, fieldDir, next_surf, next_surf_dist, &history, ds, 1);
        for (int k=0; k<3; ++k)
        {
          newPt[k] = newPt[k] + fieldDir[k]*ds;
        }

        if (particleTrace == "yes")
        {
          vtkpoints->InsertNextPoint(newPt[0], newPt[1], newPt[2]);
          vtkPointCounter +=1;    
        }
        if (next_surf != 0) // Terminate fieldline trace since shadowing surface hit
        {
          //DAG->next_vol(next_surf, vol_h, vol_h);
          history.get_last_intersection(hit);
          integrator.count_hit(hit);
          LOG_INFO << "Surface " << next_surf << " hit after travelling " << s << " units";
          //history.reset();
          double heatflux = 0.0;
          integrator.store_heat_flux(i, heatflux);
          if (particleTrace == "yes")
          {

            if (vtkParticleTracks.find(branchShadowedPart) == vtkParticleTracks.end())
            {
              int staticCast = aegisVTK.multiBlockCounters.size();
              multiBlockBranch->SetBlock(staticCast, vtkParticleTracks[branchShadowedPart]); // set block 
              multiBlockBranch->GetMetaData(static_cast<int>(staticCast)) // name block
                              ->Set(vtkCompositeDataSet::NAME(), branchShadowedPart); 
              std::cout << "vtkMultiBlock Particle_track Branch Initialised - " << branchShadowedPart << std::endl;
              aegisVTK.multiBlockCounters[branchShadowedPart] = 0;
            }  
            vtkSmartPointer<vtkPolyData> polydataTrack;
            polydataTrack = aegisVTK.new_track(branchShadowedPart, vtkpoints, heatflux);
            vtkParticleTracks[branchShadowedPart]->SetBlock(aegisVTK.multiBlockCounters[branchShadowedPart], polydataTrack);
            vtkTargetHeatflux->InsertNextTuple1(heatflux);
          }
          traceEnded = true;
          break; // break loop over ray if surface hit
        }
        history.rollback_last_intersection();


        Bfield = EquData.b_field(newPt, "cart");

        if (Bfield[0] == 0) // Terminate fieldline trace since particle has left magnetic domain
        {
          LOG_INFO << "TRACE STOPPED BECAUSE LEAVING MAGNETIC FIELD";
          integrator.count_lost_ray();
          double heatflux = 0.0;
          integrator.store_heat_flux(i, heatflux);
          if (particleTrace == "yes")
          {
            if (vtkParticleTracks.find(branchLostPart) == vtkParticleTracks.end())
            {
              int staticCast = aegisVTK.multiBlockCounters.size();
              multiBlockBranch->SetBlock(staticCast, vtkParticleTracks[branchLostPart]); // set block 
              multiBlockBranch->GetMetaData(static_cast<int>(staticCast)) // name block
                              ->Set(vtkCompositeDataSet::NAME(), branchLostPart); 
              std::cout << "vtkMultiBlock Particle_track Branch Initialised - " << branchLostPart << std::endl;
              aegisVTK.multiBlockCounters[branchLostPart] = 0;
            }  
            vtkSmartPointer<vtkPolyData> polydataTrack;
            polydataTrack = aegisVTK.new_track(branchLostPart, vtkpoints, heatflux);
            vtkParticleTracks[branchLostPart]->SetBlock(aegisVTK.multiBlockCounters[branchLostPart], polydataTrack);
            vtkTargetHeatflux->InsertNextTuple1(heatflux);
          }

          traceEnded = true;
          break; // break if ray leaves magnetic field
        }
        else
        {
          polarPos = coordTfm::cart_to_polar(newPt,"forwards");
          Bfield = EquData.b_field_cart(Bfield, polarPos[2], 0);
          norm = sqrt(pow(Bfield[0],2) + pow(Bfield[1],2) + pow(Bfield[2],2));
          normB[0] = Bfield[0]/norm;
          normB[1] = Bfield[1]/norm;
          normB[2] = Bfield[2]/norm;

          if (Bn < 0)
          {
            fieldDir[0] = -normB[0];
            fieldDir[1] = -normB[1];
            fieldDir[2] = -normB[2];

          }
          else if (Bn > 0)
          {
            fieldDir[0] = normB[0];
            fieldDir[1] = normB[1];
            fieldDir[2] = normB[2];
          }
        }

        double R = sqrt(pow(newPt[0],2) + pow(newPt[1], 2));



        if (zSign == 1)
        {
          if (R >= EquData.rbdry)
          {
            if (newPt[2] < EquData.zcen) // break when crossing zcen
            {
              polarPos = coordTfm::cart_to_polar(newPt, "forwards");
              std::vector<double> fluxPos = coordTfm::polar_to_flux(newPt, "forwards", EquData);
              double heatflux = EquData.omp_power_dep(fluxPos[0], psol, lambda_q, Bn);
              integrator.store_heat_flux(i, heatflux);
              if (particleTrace == "yes")
              {
                if (vtkParticleTracks.find(branchDepositingPart) == vtkParticleTracks.end())
                {
                  int staticCast = aegisVTK.multiBlockCounters.size();
                  multiBlockBranch->SetBlock(staticCast, vtkParticleTracks[branchDepositingPart]); // set block 
                  multiBlockBranch->GetMetaData(static_cast<int>(staticCast)) // name block
                                  ->Set(vtkCompositeDataSet::NAME(), branchDepositingPart);
                  aegisVTK.multiBlockCounters[branchDepositingPart] = 0; 
                  std::cout << "vtkMultiBlock Particle_track Branch Initialised - " << branchDepositingPart << std::endl;
                }  
                vtkSmartPointer<vtkPolyData> polydataTrack;
                polydataTrack = aegisVTK.new_track(branchDepositingPart, vtkpoints, heatflux);
                vtkParticleTracks[branchDepositingPart]->SetBlock(aegisVTK.multiBlockCounters[branchDepositingPart], polydataTrack);
                vtkTargetHeatflux->InsertNextTuple1(heatflux);
              }
              traceEnded = true;
              break; // break if ray hits omp
            }
          }
        }
        else if (zSign == -1)
        {
          if (R >= EquData.rbdry)
          {
            if (newPt[2] > EquData.zcen)
            {
              polarPos = coordTfm::cart_to_polar(newPt, "forwards");
              std::vector<double> fluxPos = coordTfm::polar_to_flux(polarPos, "forwards", EquData);
              double heatflux = EquData.omp_power_dep(fluxPos[0], psol, lambda_q, Bn);
              integrator.store_heat_flux(i, heatflux);
              if (particleTrace == "yes")
              {
                if (vtkParticleTracks.find(branchDepositingPart) == vtkParticleTracks.end())
                {
                  int staticCast = aegisVTK.multiBlockCounters.size();
                  multiBlockBranch->SetBlock(staticCast, vtkParticleTracks[branchDepositingPart]); // set block 
                  multiBlockBranch->GetMetaData(static_cast<int>(staticCast)) // name block
                                  ->Set(vtkCompositeDataSet::NAME(), branchDepositingPart); 
                  std::cout << "vtkMultiBlock Particle_track Branch Initialised - " << branchDepositingPart << std::endl;
                  aegisVTK.multiBlockCounters[branchDepositingPart] = 0;
                }  
                vtkSmartPointer<vtkPolyData> polydataTrack;
                polydataTrack = aegisVTK.new_track(branchDepositingPart, vtkpoints, heatflux);
                vtkParticleTracks[branchDepositingPart]->SetBlock(aegisVTK.multiBlockCounters[branchDepositingPart], polydataTrack);
                vtkTargetHeatflux->InsertNextTuple1(heatflux);
              }
              traceEnded = true;
              break; // break if ray hits omp
            }

            // Field line never intersects anything
            // Terminate fieldline trace and count as 0 heatflux

          }


        }





      }
      
      if (traceEnded == false)
      {
        double heatflux = 0.0;

        if (vtkParticleTracks.find(branchMaxLengthPart) == vtkParticleTracks.end())
        {
          int staticCast = aegisVTK.multiBlockCounters.size();
          multiBlockBranch->SetBlock(staticCast, vtkParticleTracks[branchMaxLengthPart]); // set block 
          multiBlockBranch->GetMetaData(static_cast<int>(staticCast)) // name block
                          ->Set(vtkCompositeDataSet::NAME(), branchMaxLengthPart); 
          std::cout << "vtkMultiBlock Particle_track Branch Initialised - " << branchMaxLengthPart << std::endl;
          aegisVTK.multiBlockCounters[branchMaxLengthPart] = 0;
        }  
        vtkSmartPointer<vtkPolyData> polydataTrack;
        polydataTrack = aegisVTK.new_track(branchMaxLengthPart, vtkpoints, heatflux);
        vtkParticleTracks[branchMaxLengthPart]->SetBlock(aegisVTK.multiBlockCounters[branchMaxLengthPart], polydataTrack);
        vtkTargetHeatflux->InsertNextTuple1(heatflux);
        LOG_INFO << "Fieldline trace reached maximum length before intersection";
        traceEnded = true;
      }
      // store vtkpolydata for singular line



    }

    LOG_WARNING << "Number of rays launched = " << targetFacets.size();
    LOG_WARNING << "Number of shadowed ray intersections = " << integrator.raysHit;
    LOG_WARNING << "Number of rays depositing power from omp = " << integrator.raysHeatDep;
    LOG_WARNING << "Number of rays lost from magnetic domain = " << integrator.raysLost;
    LOG_WARNING << "Number of rays that reached the maximum allowed length = " 
                << aegisVTK.multiBlockCounters[branchMaxLengthPart];
    //integrator.facet_values(integrator.nRays);
    //integrator.facet_values(integrator.powFac);
    integrator.csv_out(integrator.powFac);

    integrator.piecewise_multilinear_out(integrator.powFac);

    vtkTargetUstr->GetCellData()->AddArray(vtkTargetHeatflux);

    vtkNew<vtkXMLMultiBlockDataWriter> vtkMBWriter;
    vtkMBWriter->SetFileName("particle_tracks.vtm");
    vtkMBWriter->SetInputData(multiBlockRoot);
    vtkMBWriter->Write();

   // vtkGeopolydata->GetCellData()->AddArray(vtkHeatflux);

    vtkNew<vtkUnstructuredGridWriter> vtkUstrWriter;
    vtkUstrWriter->SetFileName("out.vtk");
    vtkUstrWriter->SetInputData(vtkTargetUstr);
    vtkUstrWriter->Write();

//////////



  }



  else // No runcase specified
  {
    LOG_FATAL << "No runcase specified - please choose from 'specific', 'rayqry' or 'eqdsk'";
  }


  clock_t end = clock();
  double elapsed = double(end - start)/CLOCKS_PER_SEC;

  std::cout << "------------------------------------------------------" << std::endl;
  std::cout << "Elapsed Aegis run time = " << elapsed << std::endl;
  std::cout << "------------------------------------------------------" << std::endl;

  return 0;
}

// Updates origin array for use in ray_fire
void next_pt(double prev_pt[3], double origin[3], double next_surf_dist,
            double dir[3], std::ofstream &ray_intersect){
  // prev_pt -> Array of previous ray launch point
  // next_surf_dist -> Input distance to next surface (from ray_fire)
  // dir -> Array of direction vector of ray
  // write_stream -> filestream to write out ray_intersection points



  for (int i=0; i<3; ++i) { // loop to calculate next ray launch point
    origin[i] = prev_pt[i] + (next_surf_dist * dir[i]);
    ray_intersect << origin[i] << ' ';
  }
  ray_intersect ;
  return;
}

double * vecNorm(double vector[3]){
  static double normalised_vector[3];
  double vector_mag;

  vector_mag = sqrt(vector[0]*vector[0] + vector[1]*vector[1] + vector[2]*vector[2]);
  for (int i=0; i<3; i++){
    normalised_vector[i] = vector[i]/vector_mag;
  }
  return normalised_vector;
}


// void next_dir(double raydir[3], const int qrymax, double dir_array[][3]){
//   for (int j=0; j<qrymax; j++)
//   {
//     for (int k=0; k<3; k++)
//   {
//     raydir[k] = dir_array[j+1][k] - dir_array[j][k];
//     //LOG_WARNING << raydir[k];
//   }
//   //LOG_WARNING ;
//   }
//   return;
// }


double dot_product(double vector_a[], double vector_b[]){
   double product = 0;
   for (int i = 0; i < 3; i++)
   product = product + vector_a[i] * vector_b[i];
   return product;
}

double dot_product(std::vector<double> vector_a, std::vector<double> vector_b){
   double product = 0;
   for (int i = 0; i < 3; i++)
   product = product + vector_a[i] * vector_b[i];
   return product;
}

