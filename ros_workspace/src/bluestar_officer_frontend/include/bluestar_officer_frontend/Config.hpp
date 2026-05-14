#include <string>
#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>
#include <algorithm>

using namespace std;


class BlueStarConfig {
    public:
        char cam1ip[512];
        char cam2ip[512];
        char cam3ip[512]; 
        char cam4ip[512]; 

        char cam1_video_caps[1024];
        char cam1_audio_caps[1024];

        char cam2_video_caps[1024];
        char cam2_audio_caps[1024];

        char cam3_video_caps[1024];
        char cam3_audio_caps[1024];

        char cam4_video_caps[1024];
        char cam4_audio_caps[1024];
};