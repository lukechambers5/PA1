/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Luke Chambers
	UIN: 633005470
	Date: 9/25/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <iomanip>
#include <sstream>
using namespace std;

auto fmt_ecg = [](double v) {
    ostringstream os;
    os << setprecision(15) << defaultfloat << v;
    return os.str();
};


int main(int argc, char* argv[]) {
    int opt;
    int p = -1;
    double t = -1.0;
    int e = -1;
    int buffercap = MAX_MESSAGE;
    string filename = "";
    bool use_new_channel = false;

    while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
        switch (opt) {
            case 'p': p = atoi(optarg); break;
            case 't': t = atof(optarg); break;
            case 'e': e = atoi(optarg); break;
            case 'f': filename = optarg; break;
            case 'm': buffercap = atoi(optarg); break;
            case 'c': use_new_channel = true; break;
        }
    }

    pid_t pid = fork();
	if (pid == 0) {
		if (buffercap != MAX_MESSAGE) {
			string bc = to_string(buffercap);
			char* args[] = {
				(char*)"./server",
				(char*)"-m",
				(char*)bc.c_str(),
				nullptr
			};
			execvp(args[0], args);
		} else {
			char* args[] = {
				(char*)"./server",
				nullptr
			};
			execvp(args[0], args);
		}
		_exit(127);
	}


    FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);
    FIFORequestChannel* active_chan = &chan;

    if (use_new_channel) {
        MESSAGE_TYPE m = NEWCHANNEL_MSG;
        chan.cwrite(&m, sizeof(m));
        char newchanname[100];
        chan.cread(newchanname, sizeof(newchanname));
        active_chan = new FIFORequestChannel(newchanname, FIFORequestChannel::CLIENT_SIDE);
    }

    if (p != -1 && t >= 0.0 && e != -1) {
		datamsg req(p, t, e);
		active_chan->cwrite(&req, sizeof(datamsg));
		double val;
		active_chan->cread(&val, sizeof(double));
		cout << "For person " << p << ", at time " << t
			<< ", the value of ecg " << e << " is " << val << endl;
	}

    else if (p != -1 && t < 0.0 && e == -1) {
        ofstream out("x1.csv");
        for (int i = 0; i < 1000; i++) {
            double secs = i * 0.004;
            datamsg req1(p, secs, 1);
            active_chan->cwrite(&req1, sizeof(datamsg));
            double ecg1;
            active_chan->cread(&ecg1, sizeof(double));

            datamsg req2(p, secs, 2);
            active_chan->cwrite(&req2, sizeof(datamsg));
            double ecg2;
            active_chan->cread(&ecg2, sizeof(double));

            out << secs << "," << ecg1 << "," << ecg2 << "\n";
        }
    }
    else if (!filename.empty()) {
        mkdir("received", 0777);

        filemsg fm(0, 0);
        int len = sizeof(filemsg) + filename.size() + 1;
        char* buf = new char[len];
        memcpy(buf, &fm, sizeof(filemsg));
        strcpy(buf + sizeof(filemsg), filename.c_str());

        active_chan->cwrite(buf, len);
        __int64_t filesize;
        active_chan->cread(&filesize, sizeof(__int64_t));

        string outpath = "received/" + filename;
        ofstream outfile(outpath, ios::binary);

        __int64_t offset = 0;
        while (offset < filesize) {
            int chunk = min(buffercap, (int)(filesize - offset));
            filemsg fm_chunk(offset, chunk);
            memcpy(buf, &fm_chunk, sizeof(filemsg));
            strcpy(buf + sizeof(filemsg), filename.c_str());

            active_chan->cwrite(buf, len);
            char* recvbuf = new char[chunk];
            active_chan->cread(recvbuf, chunk);
            outfile.write(recvbuf, chunk);
            delete[] recvbuf;

            offset += chunk;
        }

        outfile.close();
        delete[] buf;
    }

    if (use_new_channel) {
        MESSAGE_TYPE q = QUIT_MSG;
        active_chan->cwrite(&q, sizeof(MESSAGE_TYPE));
        delete active_chan;
    }

    MESSAGE_TYPE m = QUIT_MSG;
    chan.cwrite(&m, sizeof(MESSAGE_TYPE));
    wait(NULL);
}
