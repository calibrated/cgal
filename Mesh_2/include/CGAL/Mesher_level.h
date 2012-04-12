// Copyright (c) 2004-2005  INRIA Sophia-Antipolis (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
// You can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
// 
//
// Author(s)     : Laurent RINEAU

#ifndef CGAL_MESHER_LEVEL_H
#define CGAL_MESHER_LEVEL_H

#ifdef MESH_3_PROFILING
  #include <CGAL/Mesh_3/Profiling_tools.h>
#endif

#ifdef CONCURRENT_MESH_3
# include <algorithm>

# include <tbb/tbb.h>

# include <CGAL/hilbert_sort.h> //CJTODO: remove?
# include <CGAL/spatial_sort.h> //CJTODO: remove?
# include <CGAL/Mesh_3/Locking_data_structures.h> // CJODO TEMP?
# ifdef CGAL_MESH_3_WORKSHARING_USES_TASKS
#   include <CGAL/Mesh_3/Worksharing_data_structures.h>
#   include <tbb/task.h>
# endif
# include <CGAL/BBox_3.h>
   
# ifdef CGAL_CONCURRENT_MESH_3_PROFILING
#   define CGAL_PROFILE
#   include <CGAL/Profile_counter.h>
# endif
  
  // CJTODO TEMP TEST
# ifdef CGAL_MESH_3_DO_NOT_LOCK_INFINITE_VERTEX
  extern bool g_is_set_cell_active;
# endif
  
#endif

namespace CGAL {


enum Mesher_level_conflict_status {
  NO_CONFLICT = 0
  , CONFLICT_BUT_ELEMENT_CAN_BE_RECONSIDERED
  , CONFLICT_AND_ELEMENT_SHOULD_BE_DROPPED
  , THE_FACET_TO_REFINE_IS_NOT_IN_ITS_CONFLICT_ZONE
#ifdef CGAL_MESH_3_LAZY_REFINEMENT_QUEUE
  , ELEMENT_WAS_A_ZOMBIE
#endif
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
  , COULD_NOT_LOCK_ZONE
  , COULD_NOT_LOCK_ELEMENT
#endif
};

struct Null_mesher_level {

  template <typename Visitor>
  void refine(Visitor) {}
  
  template <typename P, typename Z
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
    , typename MV
#endif
>
  Mesher_level_conflict_status test_point_conflict_from_superior(P, Z
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
    , MV &
#endif
    )
  { 
    return NO_CONFLICT;
  }

  bool is_algorithm_done() const
  {
    return true;
  }

  template <typename Visitor>
  bool try_to_insert_one_point(Visitor) 
  {
    return false;
  }

  template <typename Visitor>
  bool one_step(Visitor)
  {
    return false;
  }
  
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
  void add_to_TLS_lists(bool) {}
  void splice_local_lists() {}
  
  template <typename Mesh_visitor>
  void before_next_element_refinement_in_superior(Mesh_visitor visitor) {}
  void before_next_element_refinement() {}
#endif // CGAL_MESH_3_CONCURRENT_REFINEMENT
  
  std::string debug_info_class_name_impl() const
  {
    return "Null_mesher_level";
  }

  std::string debug_info() const
  {
    return "";
  }
  std::string debug_info_header() const
  {
    return "";
  }

}; // end Null_mesher_level

template <
  class Tr, /**< The triangulation type. */
  class Derived, /**< Derived class, that implements methods. */
  class Element, /**< Type of elements that this level refines. */
  class Previous, /* = Null_mesher_level, */
  /**< Previous level type, defaults to
     \c Null_mesher_level. */
  class Triangulation_traits /** Traits class that defines types for the
				 triangulation. */
  > 
class Mesher_level
{
public:
  /** Type of triangulation that is meshed. */
  typedef Tr Triangulation;
  /** Type of point that are inserted into the triangulation. */
  typedef typename Triangulation::Point Point;
  /** Type of vertex handles that are returns by insertions into the
      triangulation. */
  typedef typename Triangulation::Vertex_handle Vertex_handle;  
  /** Type of facet & cell handles */
  typedef typename Triangulation::Cell_handle Cell_handle;
  typedef typename Cell_handle::value_type Cell;
  typedef typename Triangulation::Facet Facet;
  /** Type of the conflict zone for a point that can be inserted. */
  typedef typename Triangulation_traits::Zone Zone;

  typedef Element Element_type;
  typedef Previous Previous_level;

private:
  /** \name Private member functions */
  
  /** Curiously recurring template pattern. */
  //@{
  Derived& derived()
  {
    return static_cast<Derived&>(*this);
  }

  const Derived& derived() const
  {
    return static_cast<const Derived&>(*this);
  }
  //@}
  
  /// debug info: class name
  std::string debug_info_class_name() const
  {
    return derived().debug_info_class_name_impl();
  }

  /** \name Private member datas */

  Previous& previous_level; /**< The previous level of the refinement
                                    process. */
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
  Mesh_3::LockDataStructureType *m_lock_ds;
  Mesh_3::WorksharingDataStructureType *m_worksharing_ds;
#endif

#ifdef CGAL_MESH_3_WORKSHARING_USES_TASKS
  tbb::task *m_empty_root_task;
#endif

#ifdef MESH_3_PROFILING
protected:
  WallClockTimer m_timer;
#endif

public:
  typedef Mesher_level<Tr,
                       Derived,
                       Element,
                       Previous_level,
		       Triangulation_traits> Self;

  /** \name CONSTRUCTORS */
  Mesher_level(Previous_level& previous
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
               , Mesh_3::LockDataStructureType *p_lock_ds = 0
               , Mesh_3::WorksharingDataStructureType *p_worksharing_ds = 0
#endif
              )
    : previous_level(previous)
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
    , m_lock_ds(p_lock_ds)
    , m_worksharing_ds(p_worksharing_ds)
# ifdef CGAL_MESH_3_WORKSHARING_USES_TASKS
    , m_empty_root_task(0)
# endif
#endif
  {
  }

  /** \name FUNCTIONS IMPLEMENTED IN THE CLASS \c Derived */

  /**  Access to the triangulation */
  Triangulation& triangulation()
  {
    return derived().triangulation_ref_impl();
  }

  /**  Access to the triangulation */
  const Triangulation& triangulation() const
  {
    return derived().triangulation_ref_impl();
  }

  const Previous_level& previous() const
  {
    return previous_level;
  }

  Vertex_handle insert(Point p, Zone& z)
  {
    return derived().insert_impl(p, z);
  }

  Zone conflicts_zone(const Point& p
                      , Element e
                      , bool &facet_not_in_its_cz
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
                      , bool &could_lock_zone
#endif // CGAL_MESH_3_CONCURRENT_REFINEMENT
     )
  {
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
    return derived().conflicts_zone_impl(p, e, facet_not_in_its_cz, 
                                         could_lock_zone);
#else
    return derived().conflicts_zone_impl(p, e, facet_not_in_its_cz);
#endif // CGAL_MESH_3_CONCURRENT_REFINEMENT
  }

  /** Called before the first refinement, to initialized the queue of
      elements that should be refined. */
  void scan_triangulation()
  {
    derived().scan_triangulation_impl();
  }

  /** Tells if, as regards the elements of type \c Element, the refinement is
      done. */
  bool no_longer_element_to_refine()
  {
    return derived().no_longer_element_to_refine_impl();
  }

  /** Retrieves the next element that could be refined. */
  Element get_next_element()
  {
    return derived().get_next_element_impl();
  }

  /** Remove from the list the next element that could be refined. */
  void pop_next_element()
  {
    derived().pop_next_element_impl();
  }

#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
  Point circumcenter(const Element& e)
  {
    return derived().circumcenter_impl(e);
  }

  void add_to_TLS_lists(bool add)
  {
    derived().add_to_TLS_lists_impl(add);
  }
  void splice_local_lists()
  {
    derived().splice_local_lists_impl();
  }
  
  bool no_longer_local_element_to_refine()
  {
    return derived().no_longer_local_element_to_refine_impl();
  }

  Element get_next_local_element()
  {
    return derived().get_next_local_element_impl();
  }

  void pop_next_local_element()
  {
    derived().pop_next_local_element_impl();
  }
  
  template <typename Mesh_visitor>
  void treat_local_refinement_queue(Mesh_visitor visitor)
  {
    // We treat the elements of the local (TLS) refinement queue
    while (no_longer_local_element_to_refine() == false)
    {
      typedef typename Derived::Container::Element Container_element;
      Container_element ce = derived().get_next_local_raw_element_impl().second;

      const Mesher_level_conflict_status status =
        try_lock_and_refine_element(ce, visitor);
      
      switch (status)
      {
        case NO_CONFLICT:
        case CONFLICT_AND_ELEMENT_SHOULD_BE_DROPPED:
        case ELEMENT_WAS_A_ZOMBIE:
          pop_next_local_element();
          break;
        
        case COULD_NOT_LOCK_ZONE:
        case COULD_NOT_LOCK_ELEMENT:
        case THE_FACET_TO_REFINE_IS_NOT_IN_ITS_CONFLICT_ZONE:
          //std::this_thread::yield();
          break;
      }
    }
  }
  
  template <typename Mesh_visitor>
  void before_next_element_refinement_in_superior(Mesh_visitor visitor)
  {
    derived().before_next_element_refinement_in_superior_impl(visitor);
  }
  
  template <typename Mesh_visitor>
  void before_next_element_refinement(Mesh_visitor visitor)
  {
    derived().before_next_element_refinement_impl();
    previous_level.before_next_element_refinement_in_superior(
                                        visitor.previous_level());
  }
#endif // CGAL_MESH_3_CONCURRENT_REFINEMENT
  
  /** Gives the point that should be inserted to refine the element \c e */
  Point refinement_point(const Element& e)
  {
    return derived().refinement_point_impl(e);
  }

  /** Actions before testing conflicts for point \c p and element \c e */
  template <typename Mesh_visitor>
  void before_conflicts(const Element& e, const Point& p,
			Mesh_visitor visitor)
  {
    visitor.before_conflicts(e, p);
    derived().before_conflicts_impl(e, p);
  }

  /** Tells if, as regards this level of the refinement process, if the
      point conflicts with something, and do what is needed. The return
      type is made of two booleans:
        - the first one tells if the point can be inserted,
        - in case of, the first one is \c false, the second one tells if
        the tested element should be reconsidered latter.
  */
  Mesher_level_conflict_status private_test_point_conflict(const Point& p,
							   Zone& zone)
  {
    return derived().private_test_point_conflict_impl(p, zone);
  }

  /** Tells if, as regards this level of the refinement process, if the
      point conflicts with something, and do what is needed. The return
      type is made of two booleans:
        - the first one tells if the point can be inserted,
        - in case of, the first one is \c false, the second one tells if
        the tested element should be reconsidered latter.
      This function is called by the superior level, if any.
  */
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
  template <class Mesh_visitor>
#endif
  Mesher_level_conflict_status
  test_point_conflict_from_superior(const Point& p, Zone& zone
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
      , Mesh_visitor &visitor
#endif
      )
  {
    return derived().test_point_conflict_from_superior_impl(p, zone
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
      , visitor
#endif
      );
  }

  /** 
   * Actions before inserting the point \c p in order to refine the
   * element \c e. The zone of conflicts is \c zone.
   */  
  template <class Mesh_visitor>
  void before_insertion(Element& e, const Point& p, Zone& zone, 
                        Mesh_visitor visitor)
  {
    visitor.before_insertion(e, p, zone);
    derived().before_insertion_impl(e, p, zone);
  }

  /** Actions after having inserted the point.
   *  \param vh is the vertex handle of the inserted point,
   *  \param visitor is the visitor.
   */
  template <class Mesh_visitor>
  void after_insertion(Vertex_handle vh, Mesh_visitor visitor)
  {
    derived().after_insertion_impl(vh);
    visitor.after_insertion(vh);
  }

  /** Actions after testing conflicts for point \c p and element \c e 
   *  if no point is inserted. */
  template <class Mesh_visitor>
  void after_no_insertion(const Element& e, const Point& p, Zone& zone,
			  Mesh_visitor visitor)
  {
    derived().after_no_insertion_impl(e, p, zone);
    visitor.after_no_insertion(e, p, zone);
  }

  /** \name MESHING PROCESS 
   *
   * The following functions use the functions that are implemented in the
   * derived classes.
   *
   */

  /**
   * Tells it the algorithm is done, regarding elements of type \c Element
   * or elements of previous levels.
   */
  bool is_algorithm_done()
  {
#ifdef MESH_3_PROFILING
    bool done = ( previous_level.is_algorithm_done() && 
                  no_longer_element_to_refine() );
    /*if (done)
    {
      std::cerr << "done in " << m_timer.elapsed() << " seconds." << std::endl;
      m_timer.reset();
    }*/
    return done;
#else
    return ( previous_level.is_algorithm_done() && 
             no_longer_element_to_refine() );
#endif
  }

  /** Refines elements of this level and previous levels. */
  template <class Mesh_visitor>
  void refine(Mesh_visitor visitor)
  {
    while(! is_algorithm_done() )
    {
      previous_level.refine(visitor.previous_level());
      if(! no_longer_element_to_refine() )
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
        process_a_batch_of_elements(visitor);
#else
        process_one_element(visitor);
#endif
    }
  }

  /** 
   * This function takes one element from the queue, and try to refine
   * it. It returns \c true if one point has been inserted.
   * @todo Merge with try_to_refine_element().
   */
  template <class Mesh_visitor>
  bool process_one_element(Mesh_visitor visitor)
  {
    Element e = get_next_element();

    const Mesher_level_conflict_status result 
      = try_to_refine_element(e, visitor);

    if(result == CONFLICT_AND_ELEMENT_SHOULD_BE_DROPPED)
      pop_next_element();
    return result == NO_CONFLICT;
  }

#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
  
  void get_valid_vertices_of_element(const Cell_handle &e, Vertex_handle vertices[4]) const
  {
    for (int i = 0 ; i < 4 ; ++i)
      vertices[i] = e->vertex(i);
  }
  // Among the 4 values, one of them will be Vertex_handle() (~= NULL)
  void get_valid_vertices_of_element(const Facet &e, Vertex_handle vertices[4]) const
  {
    for (int i = 0 ; i < 4 ; ++i)
      vertices[i] = (i != e.second ? e.first->vertex(i) : Vertex_handle());
  }
  
  Cell_handle get_cell_from_element(const Cell_handle &e) const
  {
    return e;
  }
  Cell_handle get_cell_from_element(const Facet &e) const
  {
    return e.first;
  }

  void unlock_all_thread_local_elements()
  {
    if (m_lock_ds)
    {
# ifdef CGAL_MESH_3_LOCKING_STRATEGY_SIMPLE_GRID_LOCKING
      m_lock_ds->unlock_all_tls_locked_cells();
# elif defined(CGAL_MESH_3_LOCKING_STRATEGY_CELL_LOCK)
      std::vector<std::pair<void*, unsigned int> >::iterator it = g_tls_locked_cells.local().begin();
      std::vector<std::pair<void*, unsigned int> >::iterator it_end = g_tls_locked_cells.local().end();
      for( ; it != it_end ; ++it)
      {
        Cell_handle c(static_cast<Cell*>(it->first));
        // Not zombie?
        if( c->get_erase_counter() == it->second )
          c->unlock();
      }
      g_tls_locked_cells.local().clear();
# endif
    }
  }

  template <typename Container_element, typename Mesh_visitor>
  void enqueue_task(const Container_element &ce, Mesh_visitor visitor)
  {
    CGAL_assertion(m_empty_root_task != 0);

    m_worksharing_ds->enqueue_work(
      [&, ce, visitor]()
      {
        Mesher_level_conflict_status status;
        do
        {
          status = try_lock_and_refine_element(ce, visitor);
        }
        while (status != NO_CONFLICT
          && status != CONFLICT_AND_ELEMENT_SHOULD_BE_DROPPED
          && status != ELEMENT_WAS_A_ZOMBIE);
            
        before_next_element_refinement(visitor);

        // Finally we add the new local bad_elements to the feeder
        while (no_longer_local_element_to_refine() == false)
        {
          Container_element elt = derived().get_next_local_raw_element_impl().second;
          pop_next_local_element();
          enqueue_task(elt, visitor);
        } 
      },
      *m_empty_root_task,
      circumcenter(derived().extract_element_from_container_value(ce)));
  }

  /** 
    * This function takes N elements from the queue, and try to refine
    * it in parallel.
    */
  template <class Mesh_visitor>
  void process_a_batch_of_elements(Mesh_visitor visitor)
  {
    typedef typename Derived::Container::Element Container_element;
    typedef typename Derived::Container::Quality Container_quality;

  //=======================================================
  //================= PARALLEL_FOR?
  //=======================================================

# ifdef CGAL_MESH_3_WORKSHARING_USES_PARALLEL_FOR
    /*std::pair<Container_quality, Container_element>
      raw_elements[ELEMENT_BATCH_SIZE];*/
    std::vector<Container_element> container_elements;
    container_elements.reserve(ELEMENT_BATCH_SIZE);
    std::vector<Point> circumcenters;
    circumcenters.reserve(ELEMENT_BATCH_SIZE);
    std::vector<std::ptrdiff_t> indices;
    indices.reserve(ELEMENT_BATCH_SIZE);
    
    /*int batch_size = ELEMENT_BATCH_SIZE;
    if (debug_info_class_name() == "Refine_facets_3")
      batch_size = 1;*/

    size_t iElt = 0;
    for( ; 
          iElt < ELEMENT_BATCH_SIZE && !no_longer_element_to_refine() ; 
          ++iElt )
    {
      //raw_elements[iElt] = derived().get_next_raw_element_impl();
      //container_elements[iElt] = derived().get_next_raw_element_impl().second;
      Container_element ce = derived().get_next_raw_element_impl().second;
      pop_next_element();
      container_elements.push_back(ce);
      Point cc = circumcenter( derived().extract_element_from_container_value(ce) );
      circumcenters.push_back(cc);
      indices.push_back(iElt);
    }

#   ifdef CGAL_CONCURRENT_MESH_3_VERBOSE
    std::cerr << "Refining a batch of " << iElt << " elements...";
#   endif
    
    // Doesn't help much
    //typedef Spatial_sort_traits_adapter_3<Tr::Geom_traits, Point*> Search_traits;
    //spatial_sort( indices.begin(), indices.end(),
    //              Search_traits(&(circumcenters[0])) );
    //hilbert_sort( indices.begin(), indices.end(), Hilbert_sort_median_policy(), 
    //              Search_traits(&(circumcenters[0])) );
    //std::random_shuffle(indices.begin(), indices.end());

    // CJTODO: lambda functions OK?
    // Parallel?
    // CJTODO: TEST
    if (iElt > 20)
    {
      //g_is_set_cell_active = false;
      previous_level.add_to_TLS_lists(true);
      add_to_TLS_lists(true);
      tbb::parallel_for(
        tbb::blocked_range<size_t>( 0, iElt, MESH_3_REFINEMENT_GRAINSIZE ),
        [&] (const tbb::blocked_range<size_t>& r)
        {
          for( size_t i = r.begin() ; i != r.end() ; )
          {
            std::ptrdiff_t index = indices[i];
            Container_element ce = container_elements[index];

            const Mesher_level_conflict_status status = 
              try_lock_and_refine_element(ce, visitor);
            
            switch (status)
            {
              case NO_CONFLICT:
              case CONFLICT_AND_ELEMENT_SHOULD_BE_DROPPED:
              case ELEMENT_WAS_A_ZOMBIE:
                ++i;
                break;

              case COULD_NOT_LOCK_ZONE:
              {
                // Swap indices[i] and indices[i+1]
                if (i+1 != r.end())
                {
                  ptrdiff_t tmp = indices[i+1];
                  indices[i+1] = indices[i];
                  indices[i] = tmp;
                }
                
                // CJTODO: TEST THAT
                // Swap indices[i] and indices[last]
                /*ptrdiff_t tmp = indices[r.end() - 1];
                indices[r.end() - 1] = indices[i];
                indices[i] = tmp;*/
                break;
              }
              
              case COULD_NOT_LOCK_ELEMENT:
                // We retry it now
              case THE_FACET_TO_REFINE_IS_NOT_IN_ITS_CONFLICT_ZONE:
                // We retry it since we switched to exact computation
                // for the adjacent cells circumcenters
                break;
            }
            
            before_next_element_refinement(visitor);
          }
        }
      );
      splice_local_lists();
      //previous_level.splice_local_lists(); // useless
      previous_level.add_to_TLS_lists(false);
      add_to_TLS_lists(false);
      //g_is_set_cell_active = true;
    }
    // Go sequential
    else
    {
      for (int i = 0 ; i < iElt ; )
      {
        std::ptrdiff_t index = indices[i];

        Derived &derivd = derived();
        //Container_element ce = raw_elements[index].second;
        Container_element ce = container_elements[index];
        if( !derivd.is_zombie(ce) )
        {
          // Lock the element area on the grid
          Element element = derivd.extract_element_from_container_value(ce);
          
          const Mesher_level_conflict_status result 
            = try_to_refine_element(element, visitor);

          if (result != CONFLICT_BUT_ELEMENT_CAN_BE_RECONSIDERED
            && result != THE_FACET_TO_REFINE_IS_NOT_IN_ITS_CONFLICT_ZONE)
          {
            ++i;
          }
        }
        else
        {
          ++i;
        }
        // Unlock
        unlock_all_thread_local_elements();
      }
    }

#   ifdef CGAL_CONCURRENT_MESH_3_VERBOSE
    std::cerr << " batch done." << std::endl;
#   endif
      
  //=======================================================
  //================= PARALLEL_DO?
  //=======================================================

# elif defined(CGAL_MESH_3_WORKSHARING_USES_PARALLEL_DO)
    std::vector<Container_element> container_elements;
    container_elements.reserve(ELEMENT_BATCH_SIZE);
    
    while(!no_longer_element_to_refine())
    {
      Container_element ce = derived().get_next_raw_element_impl().second;
      pop_next_element();
      container_elements.push_back(ce);
    }

#   ifdef CGAL_CONCURRENT_MESH_3_VERBOSE
    std::cerr << "Refining elements...";
#   endif
    
    // CJTODO: lambda functions OK?
    
    //g_is_set_cell_active = false;
    previous_level.add_to_TLS_lists(true);
    add_to_TLS_lists(true);
    tbb::parallel_do(
      container_elements.begin(), container_elements.end(),
      [&] (Container_element& ce, tbb::parallel_do_feeder<Container_element>& feeder)
      {
        Mesher_level_conflict_status status;
        do 
        {
          status = try_lock_and_refine_element(ce, visitor);
        }
        while (status == COULD_NOT_LOCK_ELEMENT 
          || status == THE_FACET_TO_REFINE_IS_NOT_IN_ITS_CONFLICT_ZONE);

        switch (status)
        {
          case NO_CONFLICT:
          case CONFLICT_AND_ELEMENT_SHOULD_BE_DROPPED:
          case ELEMENT_WAS_A_ZOMBIE:
            break;

          case COULD_NOT_LOCK_ZONE:
          {
            feeder.add(ce);
            break;
          }
              
          /*case COULD_NOT_LOCK_ELEMENT:
            // We retry it now
          case THE_FACET_TO_REFINE_IS_NOT_IN_ITS_CONFLICT_ZONE:
            // We retry it since we switched to exact computation
            // for the adjacent cells circumcenters
            break;*/
        }
        
        before_next_element_refinement(visitor); 

        // Finally we add the new local bad_elements to the feeder
        while (no_longer_local_element_to_refine() == false)
        {
          typedef typename Derived::Container::Element Container_element;
          Container_element ce = derived().get_next_local_raw_element_impl().second;
          pop_next_local_element();

          feeder.add(ce);
        } 
      }
    );
    splice_local_lists();
    CGAL_assertion(no_longer_element_to_refine());
    //previous_level.splice_local_lists(); // useless
    previous_level.add_to_TLS_lists(false);
    add_to_TLS_lists(false);
    //g_is_set_cell_active = true;
    

#   ifdef CGAL_CONCURRENT_MESH_3_VERBOSE
    std::cerr << " done." << std::endl;
#   endif
  //=======================================================
  //================= TASKS?
  //=======================================================

# elif defined(CGAL_MESH_3_WORKSHARING_USES_TASKS)

    std::vector<Container_element> container_elements;
    container_elements.reserve(ELEMENT_BATCH_SIZE);
    
    while (!no_longer_element_to_refine())
    {
      Container_element ce = derived().get_next_raw_element_impl().second;
      pop_next_element();
      container_elements.push_back(ce);
    }
    
#   ifdef CGAL_CONCURRENT_MESH_3_VERBOSE
    std::cerr << "Refining elements...";
#   endif
    
    //g_is_set_cell_active = false;
    previous_level.add_to_TLS_lists(true);
    add_to_TLS_lists(true);
      
    m_empty_root_task = new( tbb::task::allocate_root() ) tbb::empty_task;
    m_empty_root_task->set_ref_count(1);

    std::vector<Container_element>::const_iterator it = 
      container_elements.begin();
    std::vector<Container_element>::const_iterator it_end = 
      container_elements.end();
    for( ; it != it_end ; ++it)
    {
      enqueue_task(*it, visitor);
    }
    m_empty_root_task->wait_for_all();

    std::cerr << " Flushing";
    bool keep_flushing = true;
    while (keep_flushing)
    {
      m_empty_root_task->set_ref_count(1);
      keep_flushing = m_worksharing_ds->flush_work_buffers(*m_empty_root_task);
      m_empty_root_task->wait_for_all();
      std::cerr << ".";
    }

    tbb::task::destroy(*m_empty_root_task);
    m_empty_root_task = 0;

    splice_local_lists();
    //previous_level.splice_local_lists(); // useless
    previous_level.add_to_TLS_lists(false);
    add_to_TLS_lists(false);
    //g_is_set_cell_active = true;
    

#   ifdef CGAL_CONCURRENT_MESH_3_VERBOSE
    std::cerr << " done." << std::endl;
#   endif

#endif
  //=======================================================
  //================= / WORKSHARING STRATEGY
  //=======================================================

  }

  /** 
   * This function takes N elements from the queue, and try to refine
   * it in parallel.
   */
  /*template <class Mesh_visitor>
  void process_a_batch_of_elements(Mesh_visitor visitor)
  {
    derived().process_a_batch_of_elements_impl(visitor);
  }*/
#endif

  template <class Mesh_visitor>
  Mesher_level_conflict_status
  try_to_refine_element(Element e, Mesh_visitor visitor)
  {
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
    // CJTODO TEMP
    //Global_mutex_type::scoped_lock lock;
#endif
     
    const Point& p = refinement_point(e);

# ifdef CGAL_MESH_3_VERY_VERBOSE
    std::cerr << "Trying to insert point: " << p << std::endl;
#endif
    
    
//=========================================
//==== Simple Grid locking
//=========================================
#if defined(CGAL_MESH_3_CONCURRENT_REFINEMENT) && \
    defined(CGAL_MESH_3_LOCKING_STRATEGY_SIMPLE_GRID_LOCKING)

    Mesher_level_conflict_status result;
    Zone zone;

    before_conflicts(e, p, visitor);
      
    bool could_lock_zone;
    bool facet_not_in_its_cz = false;
    zone = conflicts_zone(p, e, facet_not_in_its_cz, could_lock_zone);
      
    if (!could_lock_zone)
      result = COULD_NOT_LOCK_ZONE;
    else if (facet_not_in_its_cz)
      result = THE_FACET_TO_REFINE_IS_NOT_IN_ITS_CONFLICT_ZONE;
    else
      result = test_point_conflict(p, zone, visitor);

//=========================================
//==== NOT Simple Grid locking
//=========================================
#else
    
    before_conflicts(e, p, visitor);

    //=========== Concurrent? =============
#  ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
    bool could_lock_zone;
    bool facet_not_in_its_cz = false;
    Zone zone = conflicts_zone(p, e, facet_not_in_its_cz, could_lock_zone);
    Mesher_level_conflict_status result;
    if (!could_lock_zone)
      result = COULD_NOT_LOCK_ZONE
    else if (facet_not_in_its_cz)
      result = THE_FACET_TO_REFINE_IS_NOT_IN_ITS_CONFLICT_ZONE;
    else
      result = test_point_conflict(p, zone, visitor);

    //=========== or not? =================
#  else
    bool facet_not_in_its_cz = false;
    Zone zone = conflicts_zone(p, e, facet_not_in_its_cz);
    Mesher_level_conflict_status result;
    if (facet_not_in_its_cz)
      result = THE_FACET_TO_REFINE_IS_NOT_IN_ITS_CONFLICT_ZONE;
    else
      result = test_point_conflict(p, zone);
#  endif

#endif
//=========================================
//==== / Simple Grid locking
//=========================================
      
#ifdef CGAL_MESHES_DEBUG_REFINEMENT_POINTS
    std::cerr << "(" << p << ") ";
    switch( result )
    {
    case NO_CONFLICT:
      std::cerr << "accepted\n";
      break;
    case CONFLICT_BUT_ELEMENT_CAN_BE_RECONSIDERED:
      std::cerr << "rejected (temporarily)\n";
      break;
    case CONFLICT_AND_ELEMENT_SHOULD_BE_DROPPED:
      std::cerr << "rejected (permanent)\n";
      break;
    case THE_FACET_TO_REFINE_IS_NOT_IN_THE_CONFLICT_ZONE:
      std::cerr << "the facet to refine was not in the conflict zone "
        "(switching to exact)\n";
      break;
# ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
    case COULD_NOT_LOCK_ZONE:
      std::cerr << "could not lock zone\n";
      break;
# endif
    }
#endif
   
    if(result == NO_CONFLICT)
    {
      before_insertion(e, p, zone, visitor);
     
      Vertex_handle vh = insert(p, zone);
       
#  ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
    // CJTODO TEMP
    /*if (!lock.try_acquire(g_global_mutex))
      return COULD_NOT_LOCK_ZONE;*/
#  endif

#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
      if (vh == Vertex_handle())
      {
        after_no_insertion(e, p, zone, visitor);
        result = COULD_NOT_LOCK_ZONE;
      }
      else
#endif
      {
        after_insertion(vh, visitor);
      }
    }
    else 
    {
      after_no_insertion(e, p, zone, visitor);
    }
    
    return result;
  }


              
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
  template <typename Container_element, typename Mesh_visitor>
  Mesher_level_conflict_status
  try_lock_and_refine_element(const Container_element &ce, Mesh_visitor visitor)
  {
# ifdef CGAL_CONCURRENT_MESH_3_PROFILING
    static Profile_branch_counter_3 bcounter(
      std::string("early withdrawals / late withdrawals / successes [") + debug_info_class_name() + "]");
# endif

    Mesher_level_conflict_status result;
    Derived &derivd = derived();
    if( !derivd.is_zombie(ce) )
    {
      // Lock the element area on the grid
      Element element = derivd.extract_element_from_container_value(ce);
      bool locked = triangulation().try_lock_element(
                        element, MESH_3_FIRST_GRID_LOCK_RADIUS);

      if( locked )
      {
        // Test it again as it may have changed in the meantime
        if( !derivd.is_zombie(ce) )
        {
          result = try_to_refine_element(element, visitor);

          //lock.release();

          if (result == CONFLICT_BUT_ELEMENT_CAN_BE_RECONSIDERED
            || result == THE_FACET_TO_REFINE_IS_NOT_IN_ITS_CONFLICT_ZONE)
          {
# ifdef CGAL_CONCURRENT_MESH_3_PROFILING
            ++bcounter; // It's not a withdrawal
# endif
            // We try it again right now!
          }
          else if (result == COULD_NOT_LOCK_ZONE)
          {
# ifdef CGAL_CONCURRENT_MESH_3_PROFILING
            bcounter.increment_branch_1(); // THIS is a late withdrawal!
# endif
          }
          else
          {
# ifdef CGAL_CONCURRENT_MESH_3_PROFILING
            ++bcounter;
# endif
          }
        }
        else
        {
          result = ELEMENT_WAS_A_ZOMBIE;
        }

        // Unlock
        unlock_all_thread_local_elements();
      }
      // else, we try it again
      else
      {
# ifdef CGAL_CONCURRENT_MESH_3_PROFILING
        bcounter.increment_branch_2(); // THIS is an early withdrawal!
# endif
        // Unlock
        unlock_all_thread_local_elements();

        std::this_thread::yield();
        result = COULD_NOT_LOCK_ELEMENT;
      }
    }
    else
    {
      result = ELEMENT_WAS_A_ZOMBIE;
    }

    return result;
  }
#endif
  
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
  template <class Mesh_visitor>
#endif
  /** Return (can_split_the_element, drop_element). */
  Mesher_level_conflict_status
  test_point_conflict(const Point& p, Zone& zone
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
      , Mesh_visitor &visitor
#endif
  )
  {
    const Mesher_level_conflict_status result =
      previous_level.test_point_conflict_from_superior(p, zone
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
      , visitor.previous_level()
#endif
      );

    if( result != NO_CONFLICT )
      return result;
    return private_test_point_conflict(p, zone);
  }

  /** \name STEP BY STEP FUNCTIONS */

  /**
   * Inserts exactly one point, if possible, and returns \c false if no
   * point has been inserted because the algorithm is done.
   */
  template <class Mesh_visitor>
  bool try_to_insert_one_point(Mesh_visitor visitor)
  {
    while(! is_algorithm_done() )
    {
      if( previous_level.try_to_insert_one_point(visitor.previous_level()) )
        return true;
      if(! no_longer_element_to_refine() )
        if( process_one_element(visitor) )
          return true;
    }
    return false;
  }

  /**
   * Applies one step of the algorithm: tries to refine one element of
   * previous level or one element of this level. Return \c false iff 
   * <tt> is_algorithm_done()==true </tt>.
   */
  template <class Mesh_visitor>
  bool one_step(Mesh_visitor visitor)
  {
    if( ! previous_level.is_algorithm_done() )
      previous_level.one_step(visitor.previous_level());
    else
      if( ! no_longer_element_to_refine() )
      {
#ifdef CGAL_MESH_3_CONCURRENT_REFINEMENT
        process_a_batch_of_elements(visitor);
#else
        process_one_element(visitor);
#endif
      }
    return ! is_algorithm_done();
  }

}; // end Mesher_level

}  // end namespace CGAL

#include <CGAL/Mesher_level_visitors.h>
#include <CGAL/Mesher_level_default_implementations.h>

#endif // CGAL_MESHER_LEVEL_H
