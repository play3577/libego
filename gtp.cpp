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


enum gtp_status_t {
  gtp_success,
  gtp_failure,
  gtp_syntax_error,
  gtp_panic,
  gtp_quit
};

class gtp_engine_t {
public:
  virtual vector <string> get_command_names () const = 0;
  virtual gtp_status_t exec_command (string command_name, 
                                             istream& params, 
                                             ostream& response) = 0;
  virtual ~gtp_engine_t () {};
};


class gtp_t : public gtp_engine_t {

  map <string, gtp_engine_t*> engine_of_cmd_name;

  string preprocess (string s) {
    ostringstream ret;
    for_each (cp, s) {
      if (*cp == 9) ret << '\32'; 
      else if (*cp > 0 && *cp <= 9) continue; 
      else if (*cp >= 11 && *cp <= 31) continue; 
      else if (*cp == 127) continue; 
      else if (*cp == '#') break;  // remove comments
      else ret << *cp;
    }
    return ret.str ();
  }

  string status_marker (gtp_status_t status) {
    switch (status) {
    case gtp_success: return "=";
    case gtp_failure: return "?";
    case gtp_syntax_error: return "?";
    case gtp_panic:   return "!";
    case gtp_quit:    return "=";
    default: assert (false); exit(1);
    }
  }

public:


  gtp_t () { 
    register_engine (*this);
  }


  void register_engine (gtp_engine_t& gtp_engine) {
    vector<string> commands = gtp_engine.get_command_names ();
    for_each (cmd_name_it, commands) {
      engine_of_cmd_name [*cmd_name_it] = &gtp_engine;
    }
  }


  bool run_file (string file_name, ostream& out = cout) {
    ifstream in (file_name.data ());
    if (in) {
      run_loop (in, out);
      in.close ();
      return true;
    } else {
      return false;
    }
  }


  void run_loop (istream& in = cin, ostream& out = cout, bool echo_commands = false) {
    string line;
    int cmd_num;
    string cmd_name;
    gtp_status_t status;

    while (true) {
      if (!getline (in, line)) break;
      if (echo_commands) out << line << endl;

      line = preprocess (line);

      istringstream line_stream (line);
      if (!(line_stream >> cmd_num )) { cmd_num = -1; line_stream.clear (); }
      if (!(line_stream >> cmd_name)) continue; // empty line - continue

      if (engine_of_cmd_name.find (cmd_name) == engine_of_cmd_name.end ()) {
        out << "? unknown command: \"" << cmd_name << "\"" << endl << endl;
        continue;
      }

      gtp_engine_t* engine = (*(engine_of_cmd_name.find (cmd_name))).second;

      ostringstream response;
      status = engine->exec_command (cmd_name, line_stream, response);
      string response_str = response.str ();

      response_str = remove_empty_lines (response_str);
      
      // make sure there is no \n or`whitespace on the end of string
      while (isspace ( *(response_str.end ()-1) )) {
        response_str.resize (response_str.size () - 1);
      }

      out << status_marker (status);

      if (cmd_num >= 0) 
        out << cmd_num;

      out << " ";

      if (status == gtp_syntax_error) 
        out << "syntax error" << endl;

      out << response_str << endl << endl;

      if (status == gtp_panic || 
          status == gtp_quit) 
        break;
      
    }
  }

public: // basic GTP commands

  virtual vector <string> get_command_names () const {
    vector <string> commands;
    commands.push_back ("help");
    commands.push_back ("list_commands");
    commands.push_back ("known_command");
    commands.push_back ("quit");
    commands.push_back ("echo");
    commands.push_back ("run_gtp_file");
    return commands;
  };

  virtual gtp_status_t exec_command (string command, 
                                     istream& params, 
                                     ostream& response) 
  {
    if (command == "help" || command == "list_commands") {
      for_each (cmd_it, engine_of_cmd_name) { // TODO iterate over map instead of separate vector
        response << (*cmd_it).first << endl;
      }
      return gtp_success;
    }

    if (command == "known_command") {
      string known_cmd;
      if (!(params >> known_cmd)) return gtp_syntax_error;

      if (engine_of_cmd_name.find (known_cmd) == engine_of_cmd_name.end ()) response << "false";
      else response << "true";

      return gtp_success; 
    }

    if (command == "quit") 
      return gtp_quit;

    if (command == "echo") {
      string buf;
      while (params.peek () == ' ')  // eat all spaces in front
        unused (params.get ());

      if (getline (params, buf))
        response << buf; // TODO this should by in STL
      return gtp_success;
    }

    if (command == "run_gtp_file") {
      // TODO error checking
      string file_name;
      params >> file_name;
      ifstream filein (file_name.data ());
      run_loop (filein, response);
      return gtp_success;
    }

    fatal_error ("wrong command in gtp_t::exec_command");
    return gtp_panic; // formality 
  }

};



class gtp_static_commands_t : public gtp_engine_t {

public:

  map <string, string> commands_and_responses;

  gtp_static_commands_t () {}

  virtual vector <string> get_command_names () const {
    vector <string> commands;
    for_each (cmd_it, commands_and_responses)
      commands.push_back ((*cmd_it).first);
    return commands;
  };


  virtual gtp_status_t exec_command (string command, istream& params, ostream& response) {
    let (cr, commands_and_responses.find (command));
    if (cr == commands_and_responses.end ()) {
      fatal_error ("wrong command in gtp_static_commands_t::exec_command");
      return gtp_panic; // formality 
    }

    response << (*cr).second;
    return gtp_success;
  }

public:
  
  void add (string command, string response) {
    commands_and_responses [command] = response;
  }

  void extend (string command, string response_ext) {
    commands_and_responses [command] = commands_and_responses [command] + response_ext;
  }

};
