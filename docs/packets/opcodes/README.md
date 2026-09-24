# Opcode pages

One file per opcode, both directions inside, the header table then one payload section per direction. Wire order is top to bottom, `var` means the offset depends on a preceding string, the encoding is marked on every string, blob sizes are hex. A family block shared by several opcodes sits in the first member, the others point at it. Generated index, `tools/opcode_pages.py index` rebuilds it and the two tables of `../PACKET_REGISTRY.md`.

| Op | S2C | C2S | System | Status |
|---|---|---|---|---|
| [0x0000](0x0000.md) | no case |  | dispatcher | NO CASE |
| [0x0001](0x0001.md) | MessageKeyBox |  | auth | PROVEN |
| [0x0002](0x0002.md) | MessageTextBox |  | msgbox | PROVEN |
| [0x0003](0x0003.md) | OpenCharacterCreatePopup |  | charcreate | PROVEN |
| [0x0004](0x0004.md) | CreateCharacterResult | CreateCharacterRequest | charcreate | PROVEN |
| [0x0005](0x0005.md) | no case |  | dispatcher | NO CASE |
| [0x0006](0x0006.md) | no case |  | dispatcher | NO CASE |
| [0x0007](0x0007.md) | LoginAccept | LoginRequest | auth | PROVEN |
| [0x0008](0x0008.md) | ignored |  | no op | NULLSUB |
| [0x0009](0x0009.md) | no case |  | dispatcher | NO CASE |
| [0x000A](0x000A.md) | ProfileRefresh |  | progression | PROVEN |
| [0x000B](0x000B.md) | PingRequest | PingReply | keepalive | PROVEN |
| [0x000C](0x000C.md) | PingTable |  | room 3D | PROVEN |
| [0x000D](0x000D.md) | RankBoardRebuild | RaceSceneLoaded | race | PROVEN |
| [0x000E](0x000E.md) | ChannelList |  | channel | PROVEN |
| [0x000F](0x000F.md) | GarageStageAck | OpenGarageRequest | garage | PROVEN |
| [0x0010](0x0010.md) | ShopStageAck | OpenShopRequest | shop | PROVEN |
| [0x0011](0x0011.md) | MenuStageAck |  | menu | PROVEN |
| [0x0012](0x0012.md) | LobbyStageAck | OpenLobbyRequest or HostStartRace | lobby | PROVEN |
| [0x0013](0x0013.md) | RoomContext plus decor |  | room 3D | PROVEN |
| [0x0014](0x0014.md) | SceneChange |  | race | PROVEN |
| [0x0015](0x0015.md) | no case |  | dispatcher | NO CASE |
| [0x0016](0x0016.md) | LicenseStageAck | OpenLicenseScreenRequest | license | PROVEN |
| [0x0017](0x0017.md) | no case |  | dispatcher | NO CASE |
| [0x0018](0x0018.md) | no case, C2S only | StageRequestWithChannel | channel | NO CASE |
| [0x0019](0x0019.md) | ServerRedirect | ReconnectRequest | redirect | PROVEN |
| [0x001A](0x001A.md) | no case |  | dispatcher | NO CASE |
| [0x001B](0x001B.md) | OwnedCharacterList |  | inventory | PROVEN |
| [0x001C](0x001C.md) | OwnedKartList |  | inventory | PROVEN |
| [0x001D](0x001D.md) | OwnedConsumableList |  | inventory | PROVEN |
| [0x001E](0x001E.md) | OwnedPartRecord |  | inventory | PROVEN |
| [0x001F](0x001F.md) | ignored |  | no op | NULLSUB |
| [0x0020](0x0020.md) | ignored |  | no op | NULLSUB |
| [0x0021](0x0021.md) | RoomMemberJoin |  | room 3D | PROVEN |
| [0x0022](0x0022.md) | RoomMemberLeave |  | room 3D | PROVEN |
| [0x0023](0x0023.md) | RoomListRowFieldUpdate |  | lobby list | PROVEN |
| [0x0024](0x0024.md) | ignored |  | no op | NULLSUB |
| [0x0025](0x0025.md) | DeadDialog37 | NamePopupActionA | dead UI | PROVEN |
| [0x0026](0x0026.md) | no case, C2S only | RoomMemberInfoRequest | room 3D | NO CASE |
| [0x0027](0x0027.md) | DeadDialog39 |  | dead UI | PROVEN |
| [0x0028](0x0028.md) | UserInfoPopup |  | social | PROVEN |
| [0x0029](0x0029.md) | no case, C2S only | NamePopupActionB | messenger | NO CASE |
| [0x002A](0x002A.md) | WhisperPromptOn |  | chat | PROVEN |
| [0x002B](0x002B.md) | WhisperPromptOff |  | chat | PROVEN |
| [0x002C](0x002C.md) | no case, C2S only | LobbyMenuSelect | lobby | NO CASE |
| [0x002D](0x002D.md) | RoomListRowAdd | CreateRoomRequest | lobby list | PROVEN |
| [0x002E](0x002E.md) | RoomListRowRemove |  | lobby list | PROVEN |
| [0x002F](0x002F.md) | no case, C2S only | JoinRoomRequest | lobby list | NO CASE |
| [0x0030](0x0030.md) | ReadyReset plus master set |  | room 3D | PROVEN |
| [0x0031](0x0031.md) | RoomListRowPairUpdate |  | lobby list | PROVEN |
| [0x0032](0x0032.md) | RoomSlotEnable |  | room 3D | PROVEN |
| [0x0033](0x0033.md) | ReadyStateChange | ReadyToggle | room 3D | PROVEN |
| [0x0034](0x0034.md) | AllReadyBroadcast |  | room 3D | PROVEN |
| [0x0035](0x0035.md) | TrackSelectBroadcast | TrackSelectRequest | room 3D | PROVEN |
| [0x0036](0x0036.md) | ignored |  | no op | NULLSUB |
| [0x0037](0x0037.md) | ignored |  | no op | NULLSUB |
| [0x0038](0x0038.md) | ignored |  | no op | NULLSUB |
| [0x0039](0x0039.md) | ModalCloseAck | RoomMemberHostAction, kick | room 3D | PROVEN |
| [0x003A](0x003A.md) | RaceGo |  | race | PROVEN |
| [0x003B](0x003B.md) | no case | LeaveRace | race | NO CASE |
| [0x003C](0x003C.md) | FinishAndReward |  | results | PROVEN |
| [0x003D](0x003D.md) | RankBroadcast |  | results | PROVEN |
| [0x003E](0x003E.md) | GridSpawn |  | race | PROVEN |
| [0x003F](0x003F.md) | DespawnRacer |  | race | PROVEN |
| [0x0040](0x0040.md) | MotionBroadcast | SelfMotionReport | motion | PROVEN |
| [0x0041](0x0041.md) | no case, C2S only | CheckpointReport | race progress | NO CASE |
| [0x0042](0x0042.md) | CameraMode or ResultBoard |  | race cam | PROVEN |
| [0x0043](0x0043.md) | ignored |  | no op | NULLSUB |
| [0x0044](0x0044.md) | LapBoardAdvance |  | race HUD | PROVEN |
| [0x0045](0x0045.md) | StandingsRow |  | standings | PROVEN |
| [0x0046](0x0046.md) | RaceScoreboard |  | results | PROVEN |
| [0x0047](0x0047.md) | ItemEffectSpawn | ItemUseRequest | items | PROVEN |
| [0x0048](0x0048.md) | ignored |  | no op | NULLSUB |
| [0x0049](0x0049.md) | ItemGrantBroadcast | ItemGrantedReport | items | PROVEN |
| [0x004A](0x004A.md) | no case |  | none | NO CASE |
| [0x004B](0x004B.md) | HomingLaunchBroadcast | HomingLaunchRequest | items | PROVEN |
| [0x004C](0x004C.md) | no case |  | none | NO CASE |
| [0x004D](0x004D.md) | ClientInfoProbe | ClientInfoBlob | anticheat | PROVEN |
| [0x004E](0x004E.md) | DelayedAckArm | DelayedAckFire | keepalive | PROVEN |
| [0x004F](0x004F.md) | no case |  | none | NO CASE |
| [0x0050](0x0050.md) | no case |  | none | NO CASE |
| [0x0051](0x0051.md) | no case |  | none | NO CASE |
| [0x0052](0x0052.md) | no case |  | none | NO CASE |
| [0x0053](0x0053.md) | no case |  | none | NO CASE |
| [0x0054](0x0054.md) | GameServerHandoff |  | redirect | PROVEN |
| [0x0055](0x0055.md) | no case |  | none | NO CASE |
| [0x0056](0x0056.md) | no case |  | none | NO CASE |
| [0x0057](0x0057.md) | LockStateRelay | LockStateReport | items | PROVEN |
| [0x0058](0x0058.md) | DriverAnimState | DriverAnimEcho | race anim | PROVEN |
| [0x0059](0x0059.md) | no case |  | none | NO CASE |
| [0x005A](0x005A.md) | no case |  | none | NO CASE |
| [0x005B](0x005B.md) | no case |  | none | NO CASE |
| [0x005C](0x005C.md) | TurtleLaunchBroadcast | TurtleLaunchRequest | items | PROVEN |
| [0x005D](0x005D.md) | no case |  | none | NO CASE |
| [0x005E](0x005E.md) | no case |  | none | NO CASE |
| [0x005F](0x005F.md) | PetReachedRelay | PetReachedReport | items | PROVEN |
| [0x0060](0x0060.md) | no case |  | none | NO CASE |
| [0x0061](0x0061.md) | no case |  | none | NO CASE |
| [0x0062](0x0062.md) | Stage13LicenseEnter | LicenseTestStart | license | PROVEN |
| [0x0063](0x0063.md) | MakeRoomPopupOpen |  | lobby UI | PROVEN |
| [0x0064](0x0064.md) | RoomTeamUpdate | RoomTeamSelect | room teams | PROVEN |
| [0x0065](0x0065.md) | TeamGaugeAdd | TeamGaugeReport | race team | PROVEN |
| [0x0066](0x0066.md) | ignored |  | no op | NULLSUB |
| [0x0067](0x0067.md) | no case, C2S only | RaceProgressScore | race progress | NO CASE |
| [0x0068](0x0068.md) | HardTeleport | RespawnResult | motion | PROVEN |
| [0x0069](0x0069.md) | CarEffectBroadcast | HitReport | items | PROVEN |
| [0x006A](0x006A.md) | MotionBlock | RaceValuePush | motion | PROVEN |
| [0x006B](0x006B.md) | no case |  | none | NO CASE |
| [0x006C](0x006C.md) | RoomInvite | RoomInviteByName | messenger | PROVEN |
| [0x006D](0x006D.md) | ignored | RoomInviteAnswer | no op | NULLSUB |
| [0x006E](0x006E.md) | ForceJoinInvitedRoom | JoinInvitedRoom | messenger | PROVEN |
| [0x006F](0x006F.md) | FriendAddResult | FriendAddByName | friends | PROVEN |
| [0x0070](0x0070.md) | FriendRequestResolved | FriendReqAccept | friends | PROVEN |
| [0x0071](0x0071.md) | FriendRequestResolvedAlt | FriendReqReject | friends | PROVEN |
| [0x0072](0x0072.md) | UserInfoBlob | UserInfoReqByName | messenger | PROVEN |
| [0x0073](0x0073.md) | FriendStatusFull | FriendStatusPoll | presence | PROVEN |
| [0x0074](0x0074.md) | FriendDelResult | FriendDelReq | friends | PROVEN |
| [0x0075](0x0075.md) | no case |  | none | NO CASE |
| [0x0076](0x0076.md) | FriendList |  | friends | PROVEN |
| [0x0077](0x0077.md) | FriendStatusPartial |  | presence | PROVEN |
| [0x0078](0x0078.md) | FriendRequestList |  | friends | PROVEN |
| [0x0079](0x0079.md) | BlockList |  | blocks | PROVEN |
| [0x007A](0x007A.md) | BlockAddResult | BlockAddByName | blocks | PROVEN |
| [0x007B](0x007B.md) | BlockDelResult | BlockDelReq | blocks | PROVEN |
| [0x007C](0x007C.md) | no case |  | none | NO CASE |
| [0x007D](0x007D.md) | SmallTalkInvite | SmallTalkReq | smalltalk | PROVEN |
| [0x007E](0x007E.md) | ignored | SmallTalkAccept | no op | NULLSUB |
| [0x007F](0x007F.md) | ignored | SmallTalkDecline | no op | NULLSUB |
| [0x0080](0x0080.md) | ignored | SmallTalkClose | no op | NULLSUB |
| [0x0081](0x0081.md) | MessengerResult | NoteSend | messenger | PROVEN |
| [0x0082](0x0082.md) | NoteAppend |  | notes | PROVEN |
| [0x0083](0x0083.md) | NoteAppendSorted |  | notes | PROVEN |
| [0x0084](0x0084.md) | NoteMarkReadAck | NoteMarkRead | notes | PROVEN |
| [0x0085](0x0085.md) | NoteDeleteAck | NoteDelete | notes | PROVEN |
| [0x0086](0x0086.md) | ignored |  | no op | NULLSUB |
| [0x0087](0x0087.md) | MissionDefinition |  | mission | PROVEN |
| [0x0088](0x0088.md) | MissionProgressList |  | mission | PROVEN |
| [0x0089](0x0089.md) | no case |  | unused | NO CASE |
| [0x008A](0x008A.md) | MissionUnlocked |  | mission | PROVEN |
| [0x008B](0x008B.md) | ignored |  | no op | NULLSUB |
| [0x008C](0x008C.md) | MissionComplete | MissionGoalReached | mission | PROVEN |
| [0x008D](0x008D.md) | ignored | PopupClose | no op | NULLSUB |
| [0x008E](0x008E.md) | no case | ScreenDataRefresh | mission | NO CASE |
| [0x008F](0x008F.md) | MissionMenuAck | OpenMissionMenu | mission | PROVEN |
| [0x0090](0x0090.md) | MissionStartAck | StartMission | mission | PROVEN |
| [0x0091](0x0091.md) | ignored |  | no op | NULLSUB |
| [0x0092](0x0092.md) | ignored |  | no op | NULLSUB |
| [0x0093](0x0093.md) | ignored |  | no op | NULLSUB |
| [0x0094](0x0094.md) | ignored |  | no op | NULLSUB |
| [0x0095](0x0095.md) | GiftInboxReceivedList |  | gift | PROVEN |
| [0x0096](0x0096.md) | GiftSentList |  | gift | PROVEN |
| [0x0097](0x0097.md) | GiftInboxReceivedListAlt |  | gift | PROVEN |
| [0x0098](0x0098.md) | GiftSendOk | GiftBuy | gift | PROVEN |
| [0x0099](0x0099.md) | GiftAppendOne |  | gift | PROVEN |
| [0x009A](0x009A.md) | GiftClaimAck | GiftClaim | gift | PROVEN |
| [0x009B](0x009B.md) | GiftDeleteAck | GiftDelete | gift | PROVEN |
| [0x009C](0x009C.md) | GiftMarkReadAck | GiftMarkRead | gift | PROVEN |
| [0x009D](0x009D.md) | OwnedCharacterAppend |  | inventory | PROVEN |
| [0x009E](0x009E.md) | OwnedKartAppend |  | inventory | PROVEN |
| [0x009F](0x009F.md) | OwnedPartAppend |  | inventory | PROVEN |
| [0x00A0](0x00A0.md) | ignored |  | no op | NULLSUB |
| [0x00A1](0x00A1.md) | ItemDrumPicked | ItemDrumPickup | race gimmick | PROVEN |
| [0x00A2](0x00A2.md) | LicenseProgressList |  | license | PROVEN |
| [0x00A3](0x00A3.md) | LicenseTestResult | LicenseTestSubmit | license | PROVEN |
| [0x00A4](0x00A4.md) | LicenseGradeSet |  | license | PROVEN |
| [0x00A5](0x00A5.md) | PositionFixBulk |  | motion | PROVEN |
| [0x00A6](0x00A6.md) | no case | BytesConsumedKeepalive | session | NO CASE |
| [0x00A7](0x00A7.md) | ReauthAccept | Reauth | session | PROVEN |
| [0x00A8](0x00A8.md) | no case |  | unused | NO CASE |
| [0x00A9](0x00A9.md) | no case |  | unused | NO CASE |
| [0x00AA](0x00AA.md) | GhostSessionStart | GhostEnter | ghost | PROVEN |
| [0x00AB](0x00AB.md) | GhostBoardHeader |  | ghost | PROVEN |
| [0x00AC](0x00AC.md) | GhostBoardTrack |  | ghost | PROVEN |
| [0x00AD](0x00AD.md) | GhostBoardOpen |  | ghost | PROVEN |
| [0x00AE](0x00AE.md) | GhostFrameCount | ReplayUploadCount | ghost | PROVEN |
| [0x00AF](0x00AF.md) | GhostFrameChunk | ReplayUploadChunk | ghost | PROVEN |
| [0x00B0](0x00B0.md) | GhostSubmitResult | GhostSubmit | ghost | PROVEN |
| [0x00B1](0x00B1.md) | no case | GhostStageBegin | ghost | NO CASE |
| [0x00B2](0x00B2.md) | no case | GhostFinalLap | ghost | NO CASE |
| [0x00B3](0x00B3.md) | no case |  | unused | NO CASE |
| [0x00B4](0x00B4.md) | ChatBroadcast | ChatSend | chat | PROVEN |
| [0x00B5](0x00B5.md) | WhisperDeliver | WhisperSend | whisper | PROVEN |
| [0x00B6](0x00B6.md) | SystemNotice |  | notice | PROVEN |
| [0x00B7](0x00B7.md) | BuyOk | Buy | shop | PROVEN |
| [0x00B8](0x00B8.md) | DeleteOk | SellDelete | shop | PROVEN |
| [0x00B9](0x00B9.md) | EquipUseAck | EquipUse | garage | PROVEN |
| [0x00BA](0x00BA.md) | UnequipAck | Unequip | garage | PROVEN |
| [0x00BB](0x00BB.md) | ignored |  | no op | NULLSUB |
| [0x00BC](0x00BC.md) | ApplyEquipmentSet |  | garage | PROVEN |
| [0x00BD](0x00BD.md) | no case |  | unused | NO CASE |
| [0x00BE](0x00BE.md) | ClearAllCatalogs |  | catalog | PROVEN |
| [0x00BF](0x00BF.md) | DriverCatalogEntry |  | catalog | PROVEN |
| [0x00C0](0x00C0.md) | KartDefinition |  | catalog | PROVEN |
| [0x00C1](0x00C1.md) | ItemDefinition |  | catalog | PROVEN |
| [0x00C2](0x00C2.md) | KartPartDefinition |  | catalog | PROVEN |
| [0x00C3](0x00C3.md) | TrackDefinition |  | catalog | PROVEN |
| [0x00C4](0x00C4.md) | ThemeDefinition |  | catalog | PROVEN |
| [0x00C5](0x00C5.md) | LicenseTestDefinition |  | license | PROVEN |
| [0x00C6](0x00C6.md) | PriceRow |  | shop | PROVEN |
| [0x00C7](0x00C7.md) | no case |  | none | NO CASE |
| [0x00C8](0x00C8.md) | no case |  | none | NO CASE |
| [0x00C9](0x00C9.md) | PlaySoundCue |  | race audio | PROVEN |
| [0x00CA](0x00CA.md) | no case |  | none | NO CASE |
| [0x00CB](0x00CB.md) | no case | ConsumableUseNotify | none | NO CASE |
| [0x00CC](0x00CC.md) | no case | KartPartUseNotify | none | NO CASE |
| [0x00CD](0x00CD.md) | HitEffectApply | HitAck or AbilityFire | race items | PROVEN |
| [0x00CE](0x00CE.md) | SystemMessage |  | chat | PROVEN |
| [0x00CF](0x00CF.md) | ItemSlotSet | ItemSlotReport | race items | PROVEN |
| [0x00D0](0x00D0.md) | AstroBalance | WalletPoll | wallet | PROVEN |
| [0x00D9](0x00D9.md) | PlayerAppearanceUpdate | SelectCharacterAndKart | room and grid | PROVEN |
| [0x00ED](0x00ED.md) | GachaResult | GachaRoll | gacha | PROVEN |
| [0x00EE](0x00EE.md) | KartPeriodUpdate |  | race garage | PROVEN |
| [0x00EF](0x00EF.md) | no case |  | none | NO CASE |
| [0x00F0](0x00F0.md) | CarLapFlagSet |  | race | PROVEN |
| [0x00F1](0x00F1.md) | ServerClockSync |  | clock | PROVEN |
| [0x00F2](0x00F2.md) | no case | GimmickHitNotify | race gimmick | NO CASE |
| [0x00F3](0x00F3.md) | ScenarioDefinition |  | scenario | PROVEN |
| [0x00F4](0x00F4.md) | ScenarioProgressList |  | scenario | PROVEN |
| [0x00F5](0x00F5.md) | ScenarioStart | ScenarioStageSelect | scenario | PROVEN |
| [0x00F6](0x00F6.md) | ReplayFrameCountAlt |  | ghost | PROVEN |
| [0x00F7](0x00F7.md) | ReplayFrameChunkAlt |  | ghost | PROVEN |
| [0x00F8](0x00F8.md) | ScenarioResult reward | ScenarioResultReport | scenario | PROVEN |
| [0x00F9](0x00F9.md) | ScenarioProgressAppend |  | scenario | PROVEN |
| [0x00FA](0x00FA.md) | no case | ScreenDataRequest | screen init | NO CASE |
| [0x00FB](0x00FB.md) | QuestDefinition |  | quest | PROVEN |
| [0x00FC](0x00FC.md) | QuestStateList |  | quest | PROVEN |
| [0x00FD](0x00FD.md) | no case |  | none | NO CASE |
| [0x00FE](0x00FE.md) | QuestAcceptAck | QuestAccept | quest | PROVEN |
| [0x00FF](0x00FF.md) | no case |  | none | NO CASE |
| [0x0100](0x0100.md) | QuestDiscardAck | QuestDiscardReq | quest | PROVEN |
| [0x0101](0x0101.md) | QuestComplete |  | quest | PROVEN |
| [0x0102](0x0102.md) | QuestProgress | QuestProgressReport | quest | PROVEN |
| [0x0103](0x0103.md) | PetDefinition |  | pet catalog | PROVEN |
| [0x0104](0x0104.md) | OwnedPetList |  | pet inv | PROVEN |
| [0x0105](0x0105.md) | no case | no sender | hole | NO CASE |
| [0x0106](0x0106.md) | no case | no sender | hole | NO CASE |
| [0x0107](0x0107.md) | CarCraftPresetList |  | carcraft | PROVEN |
| [0x0108](0x0108.md) | CarCraftPartDefinition |  | carcraft | PROVEN |
| [0x0109](0x0109.md) | CarCraftPartInstance |  | carcraft | PROVEN |
| [0x010A](0x010A.md) | CarCraftOpenAck | CarCraftOpenReq | carcraft | PROVEN |
| [0x010B](0x010B.md) | CarCraftSaveResult | CarCraftSaveReq | carcraft | PROVEN |
| [0x010C](0x010C.md) | RoomCraftObjectDefinition |  | roomcraft | PROVEN |
| [0x010D](0x010D.md) | RoomCraftOwnedInstance |  | roomcraft | PROVEN |
| [0x010E](0x010E.md) | RoomCraftStagePush | RoomCraftOpenReq | roomcraft | PROVEN |
| [0x010F](0x010F.md) | RoomCraftSaveAck | RoomCraftSaveReq | roomcraft | PROVEN |
| [0x0110](0x0110.md) | no case | no sender | hole | NO CASE |
| [0x0111](0x0111.md) | no case | no sender | hole | NO CASE |
| [0x0112](0x0112.md) | ShopExtendOk | ShopExtendReq | shop | PROVEN |
| [0x0113](0x0113.md) | no case | no sender | hole | NO CASE |
| [0x0114](0x0114.md) | CarCraftPresetRenameAck | CarCraftPresetRenameReq | carcraft | PROVEN |
| [0x0115](0x0115.md) | ItemSetRemainingUses |  | item inv | PROVEN |
| [0x0116](0x0116.md) | WaitRoomSideListAdd |  | room 3D | PROVEN |
| [0x0117](0x0117.md) | WaitRoomSideListRemove |  | room 3D | PROVEN |
| [0x0118](0x0118.md) | RoomLockState | RoomPasswordSet | lobby list | PROVEN |
| [0x0119](0x0119.md) | PendantDefinition |  | pendant | PROVEN |
| [0x011A](0x011A.md) | PendantOwnedAppend |  | pendant | PROVEN |
| [0x011B](0x011B.md) | PendantOwnedAppendAlt |  | pendant | PROVEN |
| [0x011C](0x011C.md) | Stage22Ack | Stage22OpenReq | stage 22 | PROVEN |
| [0x011D](0x011D.md) | Stage23Ack | Stage23OpenReq | stage 23 | PROVEN |
| [0x011E](0x011E.md) | ServerRedirectStage11 |  | redirect | PROVEN |
| [0x011F](0x011F.md) | RaceTimerArm |  | race HUD | PROVEN |
| [0x0120](0x0120.md) | MissionCheckpointPath |  | mission | PROVEN |
| [0x0121](0x0121.md) | no case | MissionCheckpointReached | mission | NO CASE |
| [0x0122](0x0122.md) | MissionGo |  | mission | PROVEN |
| [0x0123](0x0123.md) | PendantEquipAck | PendantEquipReq | pendant | PROVEN |
| [0x0124](0x0124.md) | CarCraftPresetRowUpdate |  | carcraft | PROVEN |
| [0x0125](0x0125.md) | BoostReadyRelay | BoostReadyFlag | race boost | PROVEN |
| [0x0126](0x0126.md) | SystemChatLine |  | chat | PROVEN |
| [0x012E](0x012E.md) | ClientFileCheck |  | anticheat | PROVEN |
| [0x012F](0x012F.md) | RoomInvitePopup | RoomInviteBroadcastReq | room invite | PROVEN |
| [0x0130](0x0130.md) | no case | Option11Report | options | NO CASE |
| [0x0131](0x0131.md) | ItemRollStream |  | anticheat RNG | PROVEN |
| [0x0132](0x0132.md) | UserListPage | UserListPageReq | user list | PROVEN |
| [0x0133](0x0133.md) | no case | UserInfoReqById | social | NO CASE |
| [0x0134](0x0134.md) | no case |  | gacha | NO CASE |
| [0x0135](0x0135.md) | RewardPopup or LevelUpPopup |  | progression | PROVEN |
