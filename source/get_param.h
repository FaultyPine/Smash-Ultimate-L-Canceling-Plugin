#include <switch_min.h>
#include <string.h>
#include <cmath>
#include <stdio.h>
#include <stdarg.h>
#include <vector>

#include "saltysd/saltysd_core.h"
#include "saltysd/saltysd_ipc.h"
#include "saltysd/saltysd_dynamic.h"
#include "saltysd/saltysd_helper.h"

#include "useful/const_value_table.h"
#include "useful/useful.h"
#include "useful/crc32.h"
#include "app/lua_bind.h"
#include "acmd_wrapper.h"
#include "LCancels.h"

using namespace app::lua_bind;
using namespace lib;

u64 get_param_float_prev = 0;
extern bool successfullcancel[8];

/*
FOR DOING GET_PARAM REPLACEMENTS:
normally, you'd check for params in common and such with 
 if(param_type == hash40("name_of_prc_file")){
     if(param_hash == hash40("name_of_param_to_check")){
         return customValue;
     }
 }
 with fighter_param its a bit different. If you want to change a param in fighter_param, do
 if(param_hash == 0){
     if(param_type == hash40("name_of_param_to_check")){
         return customValue;
     }
 }
 if you want fighter-specific params for fighter_param stuff, check with fighter_kind

 for different param_types, see arthurs message in the ultimate modding discord here:
 https://discordapp.com/channels/447823426061860879/516439596322652161/631939317551333377
*/

bool lcancelparams(u64& boma, u64& param_type, u64& param_hash){ 
    if(param_hash == 0){
        if(
            param_type == hash40("landing_attack_air_frame_n") ||
            param_type == hash40("landing_attack_air_frame_hi") ||
            param_type == hash40("landing_attack_air_frame_lw") ||
            param_type == hash40("landing_attack_air_frame_f") ||
            param_type == hash40("landing_attack_air_frame_b")
        ){
                return true;
        }
    }
    return false;
}


/*
-------OPTION A--------- (false)
Upon successful L-Cancel, base landing lag is cut in half, otherwise, return normal landing lag

-------OPTION B--------- (true)
Universally, all base landing lag is multiplied by "universalmul". If you successfully L-Cancel, original landing lag is returned
*/


bool option_A_B = false;

float get_param_float_replace(u64 boma, u64 param_type, u64 param_hash) { //weird for fighter_param's... check param_hash against 0 and param_type for "scale" or whatever param
    //prev replace
    float (*prev_replace)(u64, u64, u64) = (float (*)(u64, u64, u64)) get_param_float_prev;
    if (prev_replace)
        prev_replace(boma, param_type, param_hash);
    
    u64 work_module = load_module(boma, 0x50);
    float (*get_param_float)(u64, u64, u64) = (float (*)(u64, u64, u64)) load_module_impl(work_module, 0x240);
    u8 kind = (u8)(*(u32*)(boma + 8) >> 28);
    //custom param checks here
    if(kind == BATTLE_OBJECT_CATEGORY_FIGHTER && lcancelparams(boma, param_type, param_hash)){ //is a fighter and accessing LL params

        if(!option_A_B){ // option a.... halving LL on successful cancel
            if(successfullcancel[get_player_number(boma)])
                return get_param_float(work_module, param_type, param_hash) / 2; //if l-canceled, divide LL by 2
            else
                return get_param_float(work_module, param_type, param_hash); //otherwise, return normal LL
        }

        else{ //option b.... all LL * universalmul   unless successful cancel, then return base LL

            if(successfullcancel[get_player_number(boma)])
                return get_param_float(work_module, param_type, param_hash); //if l-canceled, return normal LL
            else
                return get_param_float(work_module, param_type, param_hash) * 2; //otherwise return LL * 2
        }



    }
    return get_param_float(work_module, param_type, param_hash);
}

void get_param_replaces(){
    SaltySD_function_replace_sym_check_prev("_ZN3app8lua_bind32WorkModule__get_param_float_implEPNS_26BattleObjectModuleAccessorEmm", (u64)&get_param_float_replace, get_param_float_prev);
}






