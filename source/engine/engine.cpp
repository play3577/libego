//
// Copyright 2006 and onwards, Lukasz Lew
//

#include <boost/foreach.hpp>

#include "engine.hpp"

Engine::Engine () :
  random (TimeSeed()),
  root (Player::White(), Vertex::Any (), 0.0),
  sampler (playout_board, gammas),
  base_node (&root)
{
}


bool Engine::SetBoardSize (uint board_size) {
  return board_size == ::board_size;
}


void Engine::SetKomi (float komi) {
  base_board.SetKomi (komi);
}


void Engine::ClearBoard () {
  base_board.Clear ();
  root.Reset ();
}


bool Engine::Play (Move move) {
  bool ok = base_board.IsReallyLegal (move);
  if (ok) {
    base_board.PlayLegal (move);
    base_board.Dump();
  }
  return ok;
}


bool Engine::Undo () {
  bool ok = base_board.Undo ();
  return ok;
}


const Board& Engine::GetBoard () const {
  return base_board;
}


Move Engine::Genmove (Player player) {
  base_board.SetActPlayer (player);
  Move m = ChooseBestMove ();

  if (m.IsValid ()) {
    CHECK (base_board.IsReallyLegal (m));
    base_board.PlayLegal (m);
    base_board.Dump();
  }

  return m;
}


double Engine::GetStatForVertex (Vertex /*v*/) {
  return (double)(rand()%201-100)/(double)100;
}


std::string Engine::GetStringForVertex (Vertex v) {
  return "Vertex: " + v.ToGtpString();
}


Move Engine::ChooseBestMove () {
  if (Param::reset_tree_on_genmove) root.Reset ();
  Player player = base_board.ActPlayer ();
  int playouts = time_control.PlayoutCount (player);
  DoNPlayouts (playouts);

  const MctsNode& best_node = base_node->MostExploredChild (player);

  return
    best_node.SubjectiveMean() < Param::resign_mean ?
    Move::Invalid() :
    Move (player, best_node.v);
}


void Engine::DoNPlayouts (uint n) {
  SyncRoot ();
  rep (ii, n) {
    DoOnePlayout ();
  }
}


void Engine::SyncRoot () {
  // TODO replace this by FatBoard
  Board sync_board;
  Sampler sampler(sync_board, gammas);
  sampler.NewPlayout ();

  base_node = &root;
  BOOST_FOREACH (Move m, base_board.Moves ()) {
    EnsureAllLegalChildren (base_node, m.GetPlayer(), sync_board, sampler);
    base_node = base_node->FindChild (m);
    CHECK (sync_board.IsLegal (m));
    sync_board.PlayLegal (m);
    sampler.MovePlayed();
  }

  Player pl = base_board.ActPlayer();
  EnsureAllLegalChildren (base_node, pl, base_board, sampler);
  RemoveIllegalChildren (base_node, pl, base_board);
}


void Engine::DoOnePlayout () {
  PrepareToPlayout();

  // do the playout
  while (true) {
    if (playout_board.BothPlayerPass()) break;
    if (playout_board.MoveCount() >= 3*Board::kArea) return;

    Move m = Move::Invalid ();
    if (!m.IsValid()) m = ChooseMctsMove ();
    if (!m.IsValid()) m = Move (playout_board.ActPlayer (), sampler.SampleMove (random));
    PlayMove (m);
  }

  double score = Score ();
  trace.UpdateTraceRegular (score);
}


void Engine::PrepareToPlayout () {
  playout_board.Load (base_board);
  playout_moves.clear();
  sampler.NewPlayout ();

  trace.Reset (*base_node);
  playout_node = base_node;
  tree_phase = Param::tree_use;
}

Move Engine::ChooseMctsMove () {
  Player pl = playout_board.ActPlayer();

  if (!tree_phase) {
    return Move::Invalid();
  }

  if (!playout_node->has_all_legal_children [pl]) {
    if (!playout_node->ReadyToExpand ()) {
      tree_phase = false;
      return Move::Invalid();
    }
    ASSERT (pl == playout_node->player.Other());
    EnsureAllLegalChildren (playout_node, pl, playout_board, sampler);
  }

  MctsNode& uct_child = playout_node->BestRaveChild (pl);
  trace.NewNode (uct_child);
  playout_node = &uct_child;
  ASSERT (uct_child.v != Vertex::Any());
  return Move (pl, uct_child.v);
}

void Engine::EnsureAllLegalChildren (MctsNode* node, Player pl, const Board& board, const Sampler& sampler) {
  if (node->has_all_legal_children [pl]) return;
  empty_v_for_each_and_pass (&board, v, {
      // superko nodes have to be removed from the tree later
      if (board.IsLegal (pl, v)) {
      double bias = sampler.Probability (pl, v);
      node->AddChild (MctsNode(pl, v, bias));
      }
      });
  node->has_all_legal_children [pl] = true;
}


void Engine::RemoveIllegalChildren (MctsNode* node, Player pl, const Board& full_board) {
  ASSERT (node->has_all_legal_children [pl]);

  MctsNode::ChildrenList::iterator child = node->children.begin();
  while (child != node->children.end()) {
    if (child->player == pl && !full_board.IsReallyLegal (Move (pl, child->v))) {
      node->children.erase (child++);
    } else {
      ++child;
    }
  }
}


void Engine::PlayMove (Move m) {
  ASSERT (playout_board.IsLegal (m));
  playout_board.PlayLegal (m);

  trace.NewMove (m);
  sampler.MovePlayed ();

  playout_moves.push_back (m);
}


vector<Move> Engine::LastPlayout () {
  return playout_moves;
}


double Engine::Score () {
  // TODO game replay i update wszystkich modeli
  double score;
  if (tree_phase) {
    score = playout_board.TrompTaylorWinner().ToScore();
  } else {
    int sc = playout_board.PlayoutScore();
    score = Player::WinnerOfBoardScore (sc).ToScore (); // +- 1
    score += double(sc) / 10000.0; // small bonus for bigger win.
  }
  return score;
}

