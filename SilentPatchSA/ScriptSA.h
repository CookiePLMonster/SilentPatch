#ifndef __CRUNNINGSCRIPT
#define __CRUNNINGSCRIPT

#define NUM_SCRIPTS					96
#define GOSUB_STACK_SIZE			8

union SCRIPT_VAR
{
	DWORD	dwParam;
	int		iParam;
	WORD	wParam;
	BYTE	bParam;
	float	fParam;
	void*	pParam;
	char*	pcParam;
};

enum eOperandType
{
	globalVar = 2,			
	localVar = 3,			
	globalArr = 7,
	localArr = 8,
	imm8 = 4,				
	imm16 = 5,				
	imm32 = 6,				
	imm32f = 1,				
	vstring = 0x0E,			
	sstring = 9,			
	globalVarVString = 0x10,
	localVarVString = 0x11,	
	globalVarSString = 0x0A,	
	localVarSString = 0x0B,
	globalVarSArrString = 0x0C,
	localVarSArrString = 0x0D,
	globalVarVArrString = 0x12,
	localVarVArrString = 0x13,
	lstring = 0x0F
};

class CRunningScript
{
private:
	CRunningScript*			Previous;
	CRunningScript*			Next;
	char					Name[8];
	void*					BaseIP;
	void*					CurrentIP;
	void*					Stack[GOSUB_STACK_SIZE];
	WORD					SP;
	SCRIPT_VAR				LocalVar[34];
	bool					bIsActive;
	bool					bCondResult;
	bool					bUseMissionCleanup;
	bool					bIsExternal;
	bool					bTextBlockOverride;
	signed char				extrnAttachType;
	DWORD					WakeTime;
	WORD					LogicalOp;
	bool					NotFlag;
	bool					bWastedBustedCheck;
	bool					bWastedOrBusted;
	void*					SceneSkipIP;
	bool					bIsMission;
	/*	CLEO class extension */
	BYTE					scmFunction[2];
	BYTE					IsCustom;

public:
	int32_t*				GetDay_GymGlitch();
};

bool HasGameBindings_GymGlitch();

static_assert(sizeof(CRunningScript) == 0xE0, "Wrong size: CRunningScript");

#endif