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

#include "useful/raygun_printer.h"

using namespace lib;
using namespace app::lua_bind;

//used for prev_replaced functions
u64 prev_get_command_flag_cat = 0;
u64 init_settings_prev = 0;
u64 prev_is_enable_transition_term = 0;

int GLOBAL_FRAME_COUNT = 0;
int currframe = 0;
int temp_global_frame[8] = {0,0,0,0,0,0,0,0};
int lcancelframe[8] = {0,0,0,0,0,0,0,0};
bool successfullcancel[8] = {false, false, false, false, false, false, false, false};
bool color_flash_flag[8] = {false, false, false, false, false, false, false, false}; //handles getting hit while l-canceling
//frames after start of the transition to landing lag that determines when to cancel the color flash effect... 
//also used for the calculation of when you are locked out of shield, grab, spotdodge, and rolls
int cancellag = 15;

//Gives us the player number that the boma is currently addressing. Used for multi-player compatibility with variables.
int get_player_number(u64 boma){
		return WorkModule::get_int(boma, FIGHTER_INSTANCE_WORK_ID_INT_ENTRY_ID);
}
extern int get_kind(u64) asm("_ZN3app7utility8get_kindEPKNS_26BattleObjectModuleAccessorE") LINKABLE;

//checks if you've successfully l-canceled an aerial
void l_cancels(u64& boma, int& status_kind){
    if(status_kind == FIGHTER_STATUS_KIND_ATTACK_AIR){
        if(ControlModule::check_button_on_trriger(boma, CONTROL_PAD_BUTTON_GUARD) || ControlModule::check_button_on_trriger(boma, CONTROL_PAD_BUTTON_CATCH)){
            successfullcancel[get_player_number(boma)] = true;
            lcancelframe[get_player_number(boma)] = (int)MotionModule::frame(boma);
        }
        if( ( (int)MotionModule::frame(boma) - lcancelframe[get_player_number(boma)] ) > 7){
            successfullcancel[get_player_number(boma)] = false;
        }
    }
}
bool ground_fix = true; //whether or not to use ground_correct_kind fix for calc's mods
//init_settings is called at the very beginning of a transition to a new status
u64 __init_settings(u64 boma, u64 situation_kind, int param_3, u64 param_4, u64 param_5,bool param_6,int param_7,int param_8,int param_9,int param_10) {
    // Call other function replacement code if this function has been replaced
    u64 (*prev_replace)(u64, u64, int, u64, u64, bool, int, int, int, int) = (u64 (*)(u64, u64, int, u64, u64, bool, int, int, int, int)) init_settings_prev;
    if (prev_replace){
        prev_replace(boma, situation_kind, param_3, param_4, param_5, param_6, param_7, param_8, param_9, param_10);
    }

  	u64 status_module = load_module(boma, 0x40);
  	u64 (*init_settings)(u64, u64, int, u64, u64, bool, int, int, int, int) =
	  (u64 (*)(u64, u64, int, u64, u64, bool, int, int, int, int))(load_module_impl(status_module, 0x1c8));
	u8 category = (u8)(*(u32*)(boma + 8) >> 28);
    int status_kind = StatusModule::status_kind(boma);

	if (category == BATTLE_OBJECT_CATEGORY_FIGHTER) {
        //ground_correct_kind fix (for calc's ecb mod/HDR)
        if(ground_fix){
            u64 fix = param_4;
            switch (status_kind) {
                case 0x0:
                case 0x3:
                case 0x6:
                case 0x7:
                case 0x11:
                case 0x12:
                case 0x13:
                case 0x14:
                case 0x15:
                case 0x16:
                case 0x17:
                case 0x18:
                case 0x19:
                case 0x1a:
                case 0x1b:
                case 0x1c:
                case 0x1e:
                case 0x22:
                case 0x23:
                case 0x7e:
                case 0x7f:
                        fix = 1;
                        break;
            }
            param_4 = fix;
        }
        if(status_kind == FIGHTER_STATUS_KIND_ENTRY){ //reset variables on match start
            GLOBAL_FRAME_COUNT = 0;
            currframe = 0;
            for(int i = 0; i < 8; i++){
                temp_global_frame[i] = 0;
                lcancelframe[i] = 0;
                successfullcancel[i] = false;
                color_flash_flag[i] = false;
            }
        }

        //L-Cancel variable resets
        if(status_kind != FIGHTER_STATUS_KIND_ATTACK_AIR && status_kind != FIGHTER_STATUS_KIND_LANDING_ATTACK_AIR){
            successfullcancel[get_player_number(boma)] = false;
            lcancelframe[get_player_number(boma)] = 0;
        }

        // Successful L-Cancel color flash indicator
        if(successfullcancel[get_player_number(boma)] && status_kind == FIGHTER_STATUS_KIND_LANDING_ATTACK_AIR && !color_flash_flag[get_player_number(boma)] && get_kind(boma) != FIGHTER_KIND_NANA){
            temp_global_frame[get_player_number(boma)] = GLOBAL_FRAME_COUNT;
            Vector4f colorflashvec1 = { /* Red */ .x = 1.0, /* Green */ .y = 1.0, /* Blue */ .z = 1.0, /* Alpha? */ .w = 0.1}; // setting this and the next vector's .w to 1 seems to cause a ghostly effect
            Vector4f colorflashvec2 = { /* Red */ .x = 1.0, /* Green */ .y = 1.0, /* Blue */ .z = 1.0, /* Alpha? */ .w = 0.1};
            ColorBlendModule::set_main_color(boma, &colorflashvec1, &colorflashvec2, 0.7, 0.2, 25, true); //int here is opacity
            color_flash_flag[get_player_number(boma)] = true;
        }

	}
  //ORIGINAL CALL
  return init_settings(status_module, situation_kind, param_3,
  					   param_4, param_5, param_6, param_7, param_8, param_9, param_10);
}

//get_command_flag_cat is run somewhere between 4-5 times PER FRAME (is originally intended to be used to get controller input)
int get_command_flag_cat_replace(u64 boma, int category) {
    int (*prev_replace)(u64, int) = (int (*)(u64, int)) prev_get_command_flag_cat;
    if (prev_replace)
        prev_replace(boma, category);

    u64 control_module = load_module(boma, 0x48);
    int (*get_command_flag_cat)(u64, int) = (int (*)(u64, int)) load_module_impl(control_module, 0x350);
    int cat = get_command_flag_cat(control_module, category);
    int status_kind = StatusModule::status_kind(boma);
    u8 BOcategory = (u8)(*(u32*)(boma + 8) >> 28);

    if(BOcategory == BATTLE_OBJECT_CATEGORY_FIGHTER){

        //Global Frame Counter
        if((int)MotionModule::frame(boma) != currframe){ //code in here is run once per frame.... (unless theres a set_rate applied to the current move ;_; )
            currframe = (int)MotionModule::frame(boma);
            GLOBAL_FRAME_COUNT += 1;
        }

        if(status_kind != FIGHTER_STATUS_KIND_LANDING_ATTACK_AIR && color_flash_flag[get_player_number(boma)] && get_kind(boma) != FIGHTER_KIND_NANA){
            ColorBlendModule::cancel_main_color(boma, 0);
            color_flash_flag[get_player_number(boma)] = false;
        }

        l_cancels(boma, status_kind);
    }
    return cat;
}

bool disabletransterms[8] = {false, false, false, false, false, false, false, false};
//this func is a bool check for if a character is allowed to transition to specified status. returning false here means that doing that input will result in nothing
bool is_enable_transition_term_replace(u64 boma, int flag) {
    // Call other function replacement code if this function has been replaced
    bool (*prev_replace)(u64, int) = (bool (*)(u64, int)) prev_is_enable_transition_term;
    if (prev_replace){
        prev_replace(boma, flag);
    }
    // Continue with our current function replacement

    //disabling shield/grab/spotdodges/rolls for a few frames after l-cancel to prevent accidental inputs after l-canceling
    disabletransterms[get_player_number(boma)] = //disables shield, grabs, spotdodges, and rolls for a few frames
    flag == FIGHTER_STATUS_TRANSITION_TERM_ID_CONT_GUARD_ON || flag == FIGHTER_STATUS_TRANSITION_TERM_ID_CONT_CATCH || 
    flag == FIGHTER_STATUS_TRANSITION_TERM_ID_CONT_ESCAPE || flag == FIGHTER_STATUS_TRANSITION_TERM_ID_CONT_ESCAPE_F || flag == FIGHTER_STATUS_TRANSITION_TERM_ID_CONT_ESCAPE_B;
    
    if( disabletransterms[get_player_number(boma)] && (GLOBAL_FRAME_COUNT - temp_global_frame[get_player_number(boma)]) <= cancellag){
        return false;
    }
    
    
    u64 work_module = load_module(boma, 0x50);
    bool(*is_enable_transition_term)(u64, int) = (bool(*)(u64, int)) (load_module_impl(work_module, 0x180));
    bool is_enable_transition_term_Original = is_enable_transition_term(work_module, flag);
    return is_enable_transition_term_Original;
}

void LCancels(){
    SaltySD_function_replace_sym_check_prev("_ZN3app8lua_bind40ControlModule__get_command_flag_cat_implEPNS_26BattleObjectModuleAccessorEi", (u64)&get_command_flag_cat_replace, prev_get_command_flag_cat);
    SaltySD_function_replace_sym_check_prev("_ZN3app8lua_bind32StatusModule__init_settings_implEPNS_26BattleObjectModuleAccessorENS_13SituationKindEijNS_20GroundCliffCheckKindEbiiii", (u64) &__init_settings, init_settings_prev);
    SaltySD_function_replace_sym_check_prev("_ZN3app8lua_bind42WorkModule__is_enable_transition_term_implEPNS_26BattleObjectModuleAccessorEi", (u64)&is_enable_transition_term_replace, prev_is_enable_transition_term);
}