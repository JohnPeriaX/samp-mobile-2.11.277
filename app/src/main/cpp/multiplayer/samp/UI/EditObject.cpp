//
// Created by admin on 17.08.2023.
//

#include "EditObject.h"
#include <jni.h>
#include "samp/main.h"

#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"
#include "samp/GUI/gui.h"
#include "samp/Utils/JNIUtil.h"

extern CJavaWrapper *pJavaWrapper;
extern UI *pUI;
extern CNetGame *pNetGame;

void CObjectEditor::startEditPlayerAttach(int slot)
{
    if(!pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed()->GetAttachedObject(slot)){
        pUI->chat()->addDebugMessage("Invalid attach slot %d", slot);
        return;
    }
    CObjectEditor::iEditedId = slot;
    CObjectEditor::editType = TYPE_PLAYER_ATTACH;
    CObjectEditor::time = GetTickCount();

    CObjectEditor::showGui();
}

void CObjectEditor::startEditObject(uint16_t objectId)
{

}

void CObjectEditor::showGui() {
    pJavaWrapper->ShowEditObject();

    bIsToggle = true;
}


extern "C"
JNIEXPORT void JNICALL
Java_com_kurdish_roleplay_game_ui_AttachEdit_Exit(JNIEnv *env, jobject thiz) {

}

void CObjectEditor::SendOnEditAttach(int response, int index, int modelid, int bone, CVector offset, CVector rot, CVector scale){

    RakNet::BitStream bsSend;

    bsSend.Write((uint32_t)response);
    bsSend.Write((uint32_t)index);
    bsSend.Write((uint32_t)modelid);
    bsSend.Write((uint32_t)bone);
    bsSend.Write(offset);
    bsSend.Write(rot);
    bsSend.Write(scale);
    bsSend.Write((uint32_t)0);
    bsSend.Write((uint32_t)0);

    pNetGame->GetRakClient()->RPC(&RPC_EditAttachedObject, &bsSend, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_kurdish_roleplay_game_ui_AttachEdit_AttachClick(JNIEnv *env, jobject thiz, jint button_type,
                                                  jboolean button_id) {

}
extern "C"
JNIEXPORT void JNICALL
Java_com_kurdish_roleplay_game_ui_AttachEdit_Save(JNIEnv *env, jobject thiz) {

}