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

u64 prev_get_command_flag_cat = 0;
u64 init_settings_prev = 0;

int global_frame_counter = 0;
int currframe = 0;
int temp_global_frame = 0;
int lcancelframe[8] = {0,0,0,0,0,0,0,0};
bool successfullcancel[8] = {false, false, false, false, false, false, false, false};

int cancellag = 15;

int clear_buffered_action(int flag, int cmd){
        return flag & ~(flag & cmd);
}

int get_player_number(u64 boma){
		return WorkModule::get_int(boma, FIGHTER_INSTANCE_WORK_ID_INT_ENTRY_ID);
}


void l_cancels(u64& boma, int& status_kind){
    if(status_kind == FIGHTER_STATUS_KIND_ATTACK_AIR){
        if(ControlModule::check_button_on_trriger(boma, CONTROL_PAD_BUTTON_GUARD) || ControlModule::check_button_on_trriger(boma, CONTROL_PAD_BUTTON_CATCH)){
            ControlModule::clear_command(boma, true);
            successfullcancel[get_player_number(boma)] = true;
            lcancelframe[get_player_number(boma)] = (int)MotionModule::frame(boma);
        }
        if( ( (int)MotionModule::frame(boma) - lcancelframe[get_player_number(boma)] ) >= 7){
            successfullcancel[get_player_number(boma)] = false;
        }
    }
}

u64 __init_settings(u64 boma, u64 situation_kind, int param_3, u64 param_4, u64 param_5,bool param_6,int param_7,int param_8,int param_9,int param_10) {
    // Call other function replacement code if this function has been replaced
    u64 (*prev_replace)(u64, u64, int, u64, u64, bool, int, int, int, int) = (u64 (*)(u64, u64, int, u64, u64, bool, int, int, int, int)) init_settings_prev;
    if (prev_replace){
        prev_replace(boma, situation_kind, param_3, param_4, param_5, param_6, param_7, param_8, param_9, param_10);
    }
  	u64 status_module = load_module(boma, 0x40);
    u64 fix = param_4;
  	u64 (*init_settings)(u64, u64, u64, u64, u64, u64, u64, u64) =
	  (u64 (*)(u64, u64, u64, u64, u64, u64, u64, u64))(load_module_impl(status_module, 0x1c8));
	u8 category = (u8)(*(u32*)(boma + 8) >> 28);
    u64 status_kind = (u64)StatusModule::status_kind(boma);


	if (category == BATTLE_OBJECT_CATEGORY_FIGHTER) {

        //ground_correct_kind fix
        if (status_kind == (u64)FIGHTER_STATUS_KIND_APPEAL){
            fix = 1;
        }
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

        //L-Cancel variable resets
        if(status_kind != (u64)FIGHTER_STATUS_KIND_ATTACK_AIR && status_kind != (u64)FIGHTER_STATUS_KIND_LANDING_ATTACK_AIR){
            successfullcancel[get_player_number(boma)] = false;
            lcancelframe[get_player_number(boma)] = 0;
        }

        // Successful L-Cancel color flash indicator
        if(successfullcancel[get_player_number(boma)] && status_kind == (u64)FIGHTER_STATUS_KIND_LANDING_ATTACK_AIR){
            temp_global_frame = global_frame_counter;
            Vector4f colorflashvec1 = { /* Red */ .x = 1.0, /* Green */ .y = 1.0, /* Blue */ .z = 1.0, /* Alpha? */ .w = 0.1}; // setting this and the next vector's .w to 1 seems to cause a ghostly effect
            Vector4f colorflashvec2 = { /* Red */ .x = 1.0, /* Green */ .y = 1.0, /* Blue */ .z = 1.0, /* Alpha? */ .w = 0.1};
            ColorBlendModule::set_main_color(boma, &colorflashvec1, &colorflashvec2, 0.7, 0.2, 10, true);
        }

	}
  //ORIGINAL CALL
  return init_settings(status_module, situation_kind, param_3,
  					   fix, param_5, param_6, param_7, param_8);
}

/*
FIGHTER_STATUS_TRANSITION_TERM_ID_CONT_GUARD
FIGHTER_STATUS_TRANSITION_GROUP_CHK_GROUND_GUARD
*/
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
        if(status_kind == FIGHTER_STATUS_KIND_ENTRY){
            global_frame_counter = 0;
            currframe = 0;
        }
        if((int)MotionModule::frame(boma) != currframe){
            currframe = (int)MotionModule::frame(boma);
            global_frame_counter += 1;
        }

        if( (global_frame_counter - temp_global_frame) >= cancellag){ //x frames after successful l cancel
            ColorBlendModule::cancel_main_color(boma, 0);
        }

        l_cancels(boma, status_kind);

        if(successfullcancel[get_player_number(boma)] && status_kind == FIGHTER_STATUS_KIND_LANDING_ATTACK_AIR){
            AttackModule::clear_all(boma);
            if (cat & FIGHTER_PAD_CMD_CAT1_FLAG_ESCAPE) cat = clear_buffered_action(cat, FIGHTER_PAD_CMD_CAT1_FLAG_ESCAPE);
            if (cat & FIGHTER_PAD_CMD_CAT1_FLAG_CATCH) cat = clear_buffered_action(cat, FIGHTER_PAD_CMD_CAT1_FLAG_CATCH);
            if (cat & FIGHTER_PAD_CMD_CAT2_FLAG_COMMON_GUARD) cat = clear_buffered_action(cat, FIGHTER_PAD_CMD_CAT2_FLAG_COMMON_GUARD);
        }


    }
    return cat;
}

void LCancels(){
    SaltySD_function_replace_sym_check_prev("_ZN3app8lua_bind40ControlModule__get_command_flag_cat_implEPNS_26BattleObjectModuleAccessorEi", (u64)&get_command_flag_cat_replace, prev_get_command_flag_cat);
    SaltySD_function_replace_sym_check_prev("_ZN3app8lua_bind32StatusModule__init_settings_implEPNS_26BattleObjectModuleAccessorENS_13SituationKindEijNS_20GroundCliffCheckKindEbiiii", (u64) &__init_settings, init_settings_prev);
}