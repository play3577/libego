
const float kSureWinUpdate = 1.0; // TODO increase this

class MctsPlayout {
  static const bool kCheckAsserts = false;
public:
  MctsPlayout (FastRandom& random, M::Model& model) : random (random), model (model) {
  }

  void DoOnePlayout (TT& tt, const Board& base_board, Player first_player) {
    // Prepare simulation board and tree iterator.
    play_board.Load (base_board);
    play_board.SetActPlayer (first_player);
    mcmc.NewPlayout ();
    playout_moves.clear();


    if (M::Param::update) {
      // TODO this is too costly if model not in sync
      if (!model.SyncWithBoard ()) return; // TODO return false
    }
    // TODO setup nonempty as played once already

    // do the playout
    while (true) {
      if (play_board.BothPlayerPass()) break;
      if (play_board.MoveCount() >= 3*Board::kArea) return;
      Player pl = play_board.ActPlayer ();
      Vertex v  = Vertex::Any ();

      if (v == Vertex::Any ()) v = tt.ChooseMove (play_board);
      if (v == Vertex::Any ()) v = mcmc.ChooseMove (play_board);
      if (v == Vertex::Any ()) v = ChooseLocalMove ();
      if (v == Vertex::Any ()) v = play_board.RandomLightMove (pl, random);

      Move m = Move (pl, v);

      ASSERT (v.IsValid());
      ASSERT (play_board.IsLegal (pl, v));
      play_board.PlayLegal (pl, v);

      tt.NewMove (m);
      
      playout_moves.push_back (m);
      
      if (M::Param::update && play_board.PlayCount (v) == 1) {
        model.NewMove (m);
      }
    }

    // TODO game replay i update wszystkich modeli
    float score;
    if (tt.tree_phase) {
      score = play_board.TrompTaylorWinner().ToScore() * kSureWinUpdate;
    } else {
      int sc = play_board.PlayoutScore();
      score = Player::WinnerOfBoardScore (sc).ToScore (); // +- 1
      score += float(sc) / 10000.0; // small bonus for bigger win.
    }

    // update models
    tt.UpdateTraceRegular (score);

    //ASSERT (board.LastMove() == move_history[0]); // TODO remove it

    if (Param::mcmc_update) {
      // TODO remove stupid LastMove2
      mcmc.Update (score,
                   base_board.LastMove2(),
                   base_board.LastMove(),
                   playout_moves);
    }

    if (M::Param::update) {
      model.Update (score);
    }
  }

  vector<Move> LastPlayout () {
    return playout_moves;
  }

private:

  // TODO policy randomization
  Vertex ChooseLocalMove () {
    if (!Param::use_local) return Vertex::Any();
    Vertex last_v = play_board.LastVertex ();
    Player pl = play_board.ActPlayer ();

    if (last_v == Vertex::Any () || last_v == Vertex::Pass ())
      return Vertex::Any ();

    FastStack <Vertex, 8> tab;
    for_each_8_nbr (last_v, v, {
      if (play_board.IsLegal(pl, v)) {
        tab.Push (v);
      } 
    });
    
    if (tab.Size() <= 0) return Vertex::Any ();

    uint i = random.GetNextUint (tab.Size());
    return tab[i];
  }

private:
  friend class MctsGtp;
  
  // playout
  Board play_board;
  FastRandom& random;
  vector<Move> playout_moves;

public:
  Mcmc mcmc;

  M::Model& model;
};
