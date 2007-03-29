/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 *                                                                           *
 *  This file is part of Library of Effective GO routines - EGO library      *
 *                                                                           *
 *  Copyright 2006, 2007 Lukasz Lew                                          *
 *                                                                           *
 *  EGO library is free software; you can redistribute it and/or modify      *
 *  it under the terms of the GNU General Public License as published by     *
 *  the Free Software Foundation; either version 2 of the License, or        *
 *  (at your option) any later version.                                      *
 *                                                                           *
 *  EGO library is distributed in the hope that it will be useful,           *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with EGO library; if not, write to the Free Software               *
 *  Foundation, Inc., 51 Franklin St, Fifth Floor,                           *
 *  Boston, MA  02110-1301  USA                                              *
 *                                                                           *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <boost/pool/object_pool.hpp>
using namespace boost;


#ifdef NDEBUG
static const bool uct_ac   = false;
static const bool tree_ac  = false;
#endif


#ifdef DEBUG
static const bool uct_ac   = true;
static const bool tree_ac  = true;
#endif


const float initial_value            = 0.0;
const float initial_bias             = 1.0;
const float mature_bias_threshold    = initial_bias + 100.0;
const float explore_rate             = 0.2;
const uint  uct_max_depth            = 1000;
const float resign_value             = 0.99;
const uint  uct_genmove_playout_cnt  = 100000;

const float print_visit_threshold_base    = 500.0;
const float print_visit_threshold_parent  = 0.02;

// class node_t


class node_t {

public:

  v::t     v;

  float    value;
  float    bias;

  node_t*  first_child [player::cnt];         // head of list of moves of particular player 
  node_t*  sibling;                           // NULL if last child

public:
  
  #define node_for_each_child(node, pl, act_node, i) do {   \
    assertc (tree_ac, node!= NULL);                         \
    node_t* act_node;                                       \
    act_node = node->first_child [pl];                      \
    while (act_node != NULL) {                              \
      i;                                                    \
      act_node = act_node->sibling;                         \
    }                                                       \
  } while (false)

  void check () const {
    if (!tree_ac) return;
    assert (bias >= 0.0);
    v::check (v);
  }
  
  void init (v::t v) {
    this->v       = v;
    value         = initial_value;
    bias          = initial_bias;

    player_for_each (pl) 
      first_child [pl]  = NULL;

    sibling       = NULL;
  }

  void add_child (node_t* new_child, player::t pl) { // TODO sorting?
    assertc (tree_ac, new_child->sibling     == NULL); 
    player_for_each (p)
      assertc (tree_ac, new_child->first_child [p] == NULL); 

    new_child->sibling     = this->first_child [pl];
    this->first_child [pl] = new_child;
  }

  void remove_child (player::t pl, node_t* del_child) { // TODO inefficient
    node_t* act_child;
    assertc (tree_ac, del_child != NULL);

    if (first_child [pl] == del_child) {
      first_child [pl] = first_child [pl]->sibling;
      return;
    }
    
    act_child = first_child [pl];

    while (true) {
      assertc (tree_ac, act_child != NULL);
      if (act_child->sibling == del_child) {
        act_child->sibling = act_child->sibling->sibling;
        return;
      }
      act_child = act_child->sibling;
    }
  }

  template <player::t pl> float ucb (float explore_coeff) { 
    return 
      (pl == player::black ? value : -value) +
      sqrt (explore_coeff / bias);
  }

  void update (float result) {
    bias  += 1.0;
    value += (result - value) / bias; // TODO inefficiency ?
  }

  bool is_mature () { 
    return bias > mature_bias_threshold; 
  }

  bool no_children (player::t pl) {
    return first_child [pl] == NULL;
  }

  template <player::t pl> node_t* find_uct_child () {
    node_t* best_child;
    float   best_urgency;
    float   explore_coeff;

    best_child     = NULL;
    best_urgency   = - large_float;
    explore_coeff  = log (bias) * explore_rate;

    node_for_each_child (this, pl, child, {
      float child_urgency = child->ucb <pl> (explore_coeff);
      if (child_urgency > best_urgency) {
        best_urgency  = child_urgency;
        best_child    = child;
      }
    });

    assertc (tree_ac, best_child != NULL); // at least pass
    return best_child;
  }

  node_t* find_uct_child (player::t pl) {
    if (pl == player::black)
      return find_uct_child <player::black> ();
    else
      return find_uct_child <player::white> ();
  }

  node_t* find_most_explored_child (player::t pl) {
    node_t* best_child;
    float   best_bias;

    best_child     = NULL;
    best_bias      = -large_float;
    
    node_for_each_child (this, pl, child, {
      if (child->bias > best_bias) {
        best_bias     = child->bias;
        best_child    = child;
      }
    });

    assertc (tree_ac, best_child != NULL);
    return best_child;
  }

  void rec_print (ostream& out, uint depth, player::t pl) {
    rep (d, depth) out << "  ";
    out 
      << player::to_string (pl) << " " 
      << v::to_string (v) << " " 
      << value << " "
      << "(" << bias - initial_bias << ")" 
      << endl;

    player_for_each (pl)
      rec_print_children (out, depth, pl);
  }

  void rec_print_children (ostream& out, uint depth, player::t player) {
    node_t*  child_tab [v::cnt]; // rough upper bound for the number of legal move
    uint     child_tab_size;
    uint     best_child_idx;
    float    min_visit_cnt;
    
    child_tab_size  = 0;
    best_child_idx  = 0;
    min_visit_cnt   = print_visit_threshold_base + (bias - initial_bias) * print_visit_threshold_parent; // we want to be visited at least initial_bias times + some percentage of parent's visit_cnt

    // prepare for selection sort
    node_for_each_child (this, player, child, child_tab [child_tab_size++] = child);

    #define best_child child_tab [best_child_idx]

    while (child_tab_size > 0) {
      // find best child
      rep(ii, child_tab_size) {
        if ((player == player::black) == 
            (best_child->value < child_tab [ii]->value))
          best_child_idx = ii;
      }
      // rec call
      if (best_child->bias - initial_bias >= min_visit_cnt)
        child_tab [best_child_idx]->rec_print (out, depth + 1, player);      
      else break;

      // remove best
      best_child = child_tab [--child_tab_size];
    }

    #undef best_child
    
  }


};



// class tree_t


class tree_t {

public:

  object_pool <node_t>  node_pool[1];
  node_t*               history [uct_max_depth];
  uint                  history_top;

public:

  tree_t () {
    history [0] = node_pool->malloc ();
    history [0]->init (v::no_v);
  }

  void history_reset () {
    history_top = 0;
  }
  
  node_t* act_node () {
    return history [history_top];
  }
  
  void uct_descend (player::t pl) {
    history [history_top + 1] = act_node ()->find_uct_child (pl);
    history_top++;
    assertc (tree_ac, act_node () != NULL);
  }
  
  void alloc_child (player::t pl, v::t v) {
    node_t* new_node;
    new_node = node_pool->malloc ();
    new_node->init (v);
    act_node ()->add_child (new_node, pl);
  }
  
  void delete_act_node (player::t pl) {
    assertc (tree_ac, history_top > 0);
    history [history_top-1]->remove_child (pl, act_node ());
    node_pool->free (act_node ());
  }
  
  void free_subtree (node_t* parent) {
    player_for_each (pl) {
      node_for_each_child (parent, pl, child, {
        free_subtree (child);
        node_pool->free (child);
      });
    }
  }

  // TODO free history (for sync with stack_bard)
  
  void update_history (float sample) {
    rep (hi, history_top+1) 
      history [hi]->update (sample);
  }

  string to_string () { 
    ostringstream out_str;
    history [0]->rec_print (out_str, 0, player::black); 
    return out_str.str ();
  }
};


 // class uct_t


class uct_t {
public:
  
  stack_board_t*  base_board;
  tree_t          tree[1];      // TODO tree->root should be in sync with top of base_board
  
public:
  
  uct_t (stack_board_t* base_board) {
    this->base_board = base_board;
  }

  void root_ensure_children_legality (player::t pl) { // cares about superko in root (only)
    tree->history_reset ();

    assertc (uct_ac, tree->history_top == 0);
    assertc (uct_ac, tree->act_node ()->first_child [pl] == NULL);

    empty_v_for_each_and_pass (base_board->act_board (), v, {
      if (base_board->is_legal (pl, v))
        tree->alloc_child (pl, v);
    });
  }

  void do_playout (player::t first_player) flatten {
    board_t    play_board[1]; // TODO test for perfomance + memcpy
    bool       was_pass [player::cnt];
    player::t  act_player;
    v::t       v;
    
    
    play_board->load (base_board->act_board ());
    tree->history_reset ();
    
    player_for_each (pl) 
      was_pass [pl] = false; // TODO maybe there was one pass ?
    
    act_player  = first_player;
    
    do {
      if (tree->act_node ()->no_children (act_player)) { // we're finishing it
        
        // If the leaf is ready expand the tree -- add children - all potential legal v (i.e.empty)
        if (tree->act_node ()->is_mature ()) {
          empty_v_for_each_and_pass (play_board, v, {
            tree->alloc_child (act_player, v); // TODO simple ko should be handled here
            // (suicides and ko recaptures, needs to be dealt with later)
          });
          continue;            // try again
        }
        
        simple_playout::run (play_board, act_player);
        break;
        
      }
      
      tree->uct_descend (act_player);
      v = tree->act_node ()->v;
      
      if (v != v::pass && play_board->play_no_pass (act_player, v) != play_ok) {
        assertc (uct_ac, tree->act_node ()->no_children (player::other (act_player)));
        tree->delete_act_node (act_player);
        return;
      }
      
      was_pass [act_player]  = (v == v::pass);
      act_player             = player::other (act_player);
      
      if (was_pass [player::black] & was_pass [player::white]) break;
      
    } while (true);
    
    player::t winner = play_board->winner ();
    tree->update_history (1-winner-winner); // result values are 1 for black, -1 for white
  }
  

  v::t genmove (player::t player) {
    node_t* best;

    root_ensure_children_legality (player);

    rep (ii, uct_genmove_playout_cnt) do_playout (player);
    best = tree->history [0]->find_most_explored_child (player);
    assertc (uct_ac, best != NULL);

    //cerr << tree->to_string () << endl;
    if (player == player::black && best->value < -resign_value) return v::resign;
    if (player == player::white && best->value >  resign_value) return v::resign;
    return best->v;
  }
  
};