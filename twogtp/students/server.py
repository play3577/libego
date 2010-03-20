#!/usr/bin/python

import xmlrpclib
from SimpleXMLRPCServer import SimpleXMLRPCServer
import random, os, datetime, sys, traceback, threading

if len (sys.argv) != 2:
    print "Usage: %s <time_in_minutes>" % sys.argv [0]
    sys.exit (1)

uptime_minutes = float (sys.argv[1])

start_time = datetime.datetime.now()
log_dir = "/home/dokstud/lew/logi/%s" % start_time.isoformat()
os.makedirs (log_dir)
os.chdir (log_dir)


address = "students.mimuw.edu.pl", 7553


report_file = "report.csv"

#TODO list of strings
params_values = {
    "progressive_bias" : [25.0, 35.0, 50.0, 70.0, 100.0, 140.0, 200.0, 280.0, 400.0],
    "progressive_bias_decay" : [0.0625, 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0]}

params_names  = params_values.keys () #csv header

csv_header = ", ".join (params_names) + ", first_is_black, black_win, sgf_file"

params_of_series = []
games_per_series = 2

gnugo = "'gnugo-3.8 --mode gtp --chinese-rules --capture-all-dead --positional-superko --level=0'"
libego = "'/home/dokstud/lew/libego/bin/engine \"LoadGammas /home/dokstud/lew/libego/bin/3x3.gamma\" %s gtp'" 
gtp_set_command = "set"

# TODO seed, cputime


def gen_params ():
    def select_param (x):
        vals = params_values[x]
        return x, vals [random.randrange (len (vals))]
    return dict ((select_param (x) for x in params_values))


def params_to_gtp (params):
    gtp = ""
    for param_name in params_names:
        gtp += '"%s %s %f" ' % (gtp_set_command, param_name, params[param_name])
    return gtp


def format_report_line (series_id, first_is_black, black_win, sgf):
    params = params_of_series [series_id]
    csv_line = ", ".join ((str(params [param_name]) for param_name in params_names))
    # scv_line, oh yeah :)
    return csv_line + (', %d, %d, "%s"' % (first_is_black, black_win, sgf))



def get_game_setup ():
    global series_id
    params = gen_params ()
    series_id = len (params_of_series)
    params_of_series.append (params)
    ret = (log_dir,
           series_id,
           games_per_series,
           libego % params_to_gtp (params),
           gnugo
           )
    return ret

def report_game_result (series_id, result_list):
    f = open (report_file, "a")
    # TODO check for result length
    for (first_is_black, black_win, sgf) in result_list:
        print (first_is_black, black_win, sgf)
        f.write ("%s\n" % format_report_line (series_id, first_is_black, black_win, sgf))
    f.close ()
    return 0


server = SimpleXMLRPCServer (address)
threading.Timer(uptime_minutes * 60.0, server.shutdown).start()

server.register_function (get_game_setup, "get_game_setup")
server.register_function (report_game_result, "report_game_result")

print "Listening on port %d..." % address[1]

f = open (report_file, "a")
f.write ("%s\n" % csv_header)
f.close ()

server.serve_forever()