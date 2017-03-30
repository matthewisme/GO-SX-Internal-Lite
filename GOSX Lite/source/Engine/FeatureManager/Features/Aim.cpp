//
//  Aim.cpp
//  GOSX Lite
//
//  Created by Andre Kalisch on 30.03.17.
//  Copyright © 2017 Andre Kalisch. All rights reserved.
//

#include "Aim.h"


CAim::CAim() {}

void CAim::CreateMove(CUserCmd *pCmd) {
    C_CSPlayer* LocalPlayer = C_CSPlayer::GetLocalPlayer();
    if(!LocalPlayer || !LocalPlayer->IsValidLivePlayer()) {
        return;
    }

    C_BaseCombatWeapon* currentWeapon = LocalPlayer->GetActiveWeapon();
    if(!currentWeapon) {
        return;
    }

    int currentWeaponID = currentWeapon->GetWeaponEntityID();
    if (
        CWeaponManager::isC4(currentWeaponID) ||
        CWeaponManager::isKnife(currentWeaponID) ||
        CWeaponManager::isGrenade(currentWeaponID) ||
        CWeaponManager::isNonAimWeapon(currentWeaponID)
    ) {
        return;
    }

    if (
        (
            !(pCmd->buttons & IN_ATTACK) &&
            currentWeaponID != EItemDefinitionIndex::weapon_revolver
        ) ||
        (
            !(pCmd->buttons & IN_ATTACK2) &&
            currentWeaponID == EItemDefinitionIndex::weapon_revolver
        )
    ) {
        return;
    }

    C_CSPlayer* TargetEntity = FindTarget(LocalPlayer);
    if(!TargetEntity || !TargetEntity->IsValidLivePlayer()) {
        return;
    }

    StartAim(LocalPlayer, TargetEntity, pCmd);
}

C_CSPlayer* CAim::FindTarget(C_CSPlayer* LocalPlayer) {
    float fov = INIGET_FLOAT("AimHelper", "aim_fov");
    C_CSPlayer* TargetEntity = nullptr;

    Vector LocalPlayerEyePosition = LocalPlayer->GetEyePos();
    Vector LocalPlayerViewAngle = LocalPlayer->GetViewAngle();

    for(int i = 1; i < Interfaces::Engine()->GetMaxClients(); i++) {
        C_CSPlayer* PossibleTarget = C_CSPlayer::GetEntity(i);
        if(!PossibleTarget || !PossibleTarget->IsValidLivePlayer() || PossibleTarget->GetImmune()) {
            continue;
        }

        if(INIGET_BOOL("AimHelper", "team_check") && LocalPlayer->GetTeamNum() == PossibleTarget->GetTeamNum()) {
            continue;
        }

        Vector PossibleTargetHitbox = PossibleTarget->GetPredictedPosition(HITBOX_NECK);
        if(INIGET_BOOL("AimHelper", "visibility_check") && !PossibleTarget->IsVisible(LocalPlayer, PossibleTargetHitbox)) {
            continue;
        }

        float FieldOfViewToTarget = CMath::GetFov(LocalPlayerViewAngle, LocalPlayerEyePosition, PossibleTargetHitbox);

        if(FieldOfViewToTarget < fov) {
            TargetEntity = PossibleTarget;
            fov = FieldOfViewToTarget;
        }
    }

    return TargetEntity;
}

void CAim::StartAim(C_CSPlayer* LocalPlayer, C_CSPlayer* AimTarget, CUserCmd* pCmd) {
    Vector AimAngle, OldAngle;
    Interfaces::Engine()->GetViewAngles(OldAngle);
    float oldForward = pCmd->forwardmove;
    float oldSideMove = pCmd->sidemove;

    Vector TargetHitbox = AimTarget->GetPredictedPosition(HITBOX_NECK);
    Vector dir = LocalPlayer->GetEyePos() - TargetHitbox;
    
    CMath::VectorNormalize(dir);
    CMath::VectorAngles(dir, AimAngle);

    RCS(AimAngle, LocalPlayer, AimTarget, pCmd);
    Smooth(AimAngle);

    if (AimAngle != OldAngle) {
        CMath::NormalizeAngles(AimAngle);
        CMath::ClampAngle(AimAngle);
        if(AimAngle.IsValid()) {
            if(INIGET_BOOL("AimHelper", "aim_silent")) {
                pCmd->viewangles = AimAngle;
            } else {
                CMath::CorrectMovement(OldAngle, pCmd, oldForward, oldSideMove);
                Interfaces::Engine()->SetViewAngles(AimAngle);
            }
        }
    }
}

void CAim::RCS(QAngle& angle, C_CSPlayer* LocalPlayer, C_CSPlayer* TargetEntity, CUserCmd* pCmd) {
    if (!INIGET_BOOL("AimHelper", "aim_rcs")) {
        return;
    }

    if (!(pCmd->buttons & IN_ATTACK)) {
        return;
    }

    C_BaseCombatWeapon* currentWeapon = LocalPlayer->GetActiveWeapon();
    if(!currentWeapon) {
        return;
    }
    int currentWeaponID = currentWeapon->GetWeaponEntityID();

    float RcsLevel = INIGET_FLOAT("AimHelper", "aim_rcs_level");
    QAngle CurrentPunch = LocalPlayer->AimPunch();
    if (
        CWeaponManager::isRCSWeapon(currentWeaponID) &&
        CurrentPunch.x != 0.0f &&
        CurrentPunch.y != 0.0f
    ) {
        angle.x -= CurrentPunch.x * RcsLevel;
        angle.y -= CurrentPunch.y * RcsLevel;
    }
}

void CAim::Smooth(QAngle& angle)
{
    if (!INIGET_BOOL("AimHelper", "aim_smooth")) {
        return;
    }

    QAngle viewAngles = QAngle(0.f, 0.f, 0.f);
    Interfaces::Engine()->GetViewAngles(viewAngles);

    QAngle delta = angle - viewAngles;
    CMath::NormalizeAngles(delta);

    float smooth = powf(INIGET_FLOAT("AimHelper", "aim_smoothing"), 0.4f);
    smooth = std::min(0.99f, smooth);

    QAngle toChange = delta - delta * smooth;

    angle = viewAngles + toChange;
}
