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
float lcancelparams(u64& boma, u64& param_type, u64& param_hash){
    //int status_kind = StatusModule::status_kind(boma);

    if(param_hash == 0){
        if(
            param_type == hash40("landing_attack_air_frame_n") ||
            param_type == hash40("landing_attack_air_frame_hi") ||
            param_type == hash40("landing_attack_air_frame_lw") ||
            param_type == hash40("landing_attack_air_frame_f") ||
            param_type == hash40("landing_attack_air_frame_b")
        ){
            if(successfullcancel[get_player_number(boma)]){
                return 2.0; //factor to divide original LL by
            }
        }
    }
    return 1.0;
}


/*
-------OPTION A--------- (false)
Upon successful L-Cancel, base landing lag is cut in half, otherwise, return normal landing lag

-------OPTION B--------- (true)
Universally, all base landing lag is multiplied by "universalmul". If you successfully L-Cancel, original landing lag is returned
*/


bool option_A_B = false;

float get_param_float_replace(u64 module_accessor, u64 param_type, u64 param_hash) { //weird for fighter_param's... check param_hash against 0 and param_type for "scale" or whatever param
    //prev replace
    float (*prev_replace)(u64, u64, u64) = (float (*)(u64, u64, u64)) get_param_float_prev;
    if (prev_replace)
        prev_replace(module_accessor, param_type, param_hash);
    
    u64 work_module = load_module(module_accessor, 0x50);
    float (*get_param_float)(u64, u64, u64) = (float (*)(u64, u64, u64)) load_module_impl(work_module, 0x240);
    u8 kind = (u8)(*(u32*)(module_accessor + 8) >> 28);
    //custom param checks here
    if(kind == BATTLE_OBJECT_CATEGORY_FIGHTER){
        
        
        float mul = lcancelparams(module_accessor, param_type, param_hash);
        float universalmul = 1.8;
        if(successfullcancel[get_player_number(module_accessor)]){

            if(!option_A_B){ // option a.... halving LL on successful cancel
                return get_param_float(work_module, param_type, param_hash) / mul;
            }
            else{ //option b.... all LL * universalmul   unless successful cancel, then return base LL
                if(mul > 1){
                    return get_param_float(work_module, param_type, param_hash);
                }
                else{
                    return trunc(get_param_float(work_module, param_type, param_hash) * universalmul);
                }
            }
        }


    }
    return get_param_float(work_module, param_type, param_hash);
}

/*
int get_param_int_replace(u64 module_accessor, u64 param_type, u64 param_hash) {
    u64 work_module = load_module(module_accessor, 0x50);
    int (*get_param_int)(u64, u64, u64) = (int (*)(u64, u64, u64)) load_module_impl(work_module, 0x220);
    //custom param checks here

    
    return get_param_int(work_module, param_type, param_hash);
}
*/

void get_param_replaces(){
    SaltySD_function_replace_sym_check_prev("_ZN3app8lua_bind32WorkModule__get_param_float_implEPNS_26BattleObjectModuleAccessorEmm", (u64)&get_param_float_replace, get_param_float_prev);
    //SaltySD_function_replace_sym("_ZN3app8lua_bind30WorkModule__get_param_int_implEPNS_26BattleObjectModuleAccessorEmm", (u64)&get_param_int_replace);
}






