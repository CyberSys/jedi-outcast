#ifndef __TR_PUBLIC_H
#define __TR_PUBLIC_H

#include "tr_types.h"
#include "../qcommon/miniheap.h"

#define	REF_API_VERSION		9

//
// these are the functions exported by the refresh module
//
typedef struct {
	// called before the library is unloaded
	// if the system is just reconfiguring, pass destroyWindow = qfalse,
	// which will keep the screen from flashing to the desktop.
	void	(*Shutdown)( qboolean destroyWindow );

	// All data that will be used in a level should be
	// registered before rendering any frames to prevent disk hits,
	// but they can still be registered at a later time
	// if necessary.
	//
	// BeginRegistration makes any existing media pointers invalid
	// and returns the current gl configuration, including screen width
	// and height, which can be used by the client to intelligently
	// size display elements
	void	(*BeginRegistration)( glconfig_t *config );
	qhandle_t (*RegisterModel)( const char *name );
	qhandle_t (*RegisterSkin)( const char *name );
	int		  (*GetAnimationCFG)(const char *psCFGFilename, char *psDest, int iDestSize);
	qhandle_t (*RegisterShader)( const char *name );
	qhandle_t (*RegisterShaderNoMip)( const char *name );
	void	(*LoadWorld)( const char *name );

	// these two functions added to help with the new model alloc scheme...
	//
	void	(*RegisterMedia_LevelLoadBegin)(const char *psMapName, ForceReload_e eForceReload, qboolean bAllowScreenDissolve);
	void	(*RegisterMedia_LevelLoadEnd)(void);

	// the vis data is a large enough block of data that we go to the trouble
	// of sharing it with the clipmodel subsystem
	void	(*SetWorldVisData)( const byte *vis );

	// EndRegistration will draw a tiny polygon with each texture, forcing
	// them to be loaded into card memory
	void	(*EndRegistration)( void );

	// a scene is built up by calls to R_ClearScene and the various R_Add functions.
	// Nothing is drawn until R_RenderScene is called.
	void	(*ClearScene)( void );
	void	(*AddRefEntityToScene)( const refEntity_t *re );
	void	(*AddPolyToScene)( qhandle_t hShader , int numVerts, const polyVert_t *verts );
	void	(*AddLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void	(*RenderScene)( const refdef_t *fd );
	qboolean(*GetLighting)( const vec3_t org, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);

	void	(*SetColor)( const float *rgba );	// NULL = 1,1,1,1
	void	(*DrawStretchPic) ( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, qhandle_t hShader );	// 0 = white
	void	(*DrawRotatePic) ( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, float a1, qhandle_t hShader );	// 0 = white
	void	(*DrawRotatePic2) ( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, float a1, qhandle_t hShader );	// 0 = white
	void	(*LAGoggles)(void);
	void	(*Scissor) ( float x, float y, float w, float h);	// 0 = white

	// Draw images for cinematic rendering, pass as 32 bit rgba
	void	(*DrawStretchRaw) (int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);
	void	(*UploadCinematic) (int cols, int rows, const byte *data, int client, qboolean dirty);

	void	(*BeginFrame)( stereoFrame_t stereoFrame );

	// if the pointers are not NULL, timing info will be returned
	void	(*EndFrame)( int *frontEndMsec, int *backEndMsec );

	qboolean (*ProcessDissolve)(void);
	qboolean (*InitDissolve)(qboolean bForceCircularExtroWipe);


	// for use with save-games mainly...
	void	(*GetScreenShot)(byte *data, int w, int h);

	// this is so you can get access to raw pixels from a graphics format (TGA/JPG/BMP etc),
	//	currently only the save game uses it (to make raw shots for the autosaves)
	//
	byte*	(*TempRawImage_ReadFromFile)(const char *psLocalFilename, int *piWidth, int *piHeight, byte *pbReSampleBuffer, qboolean qbVertFlip);
	void	(*TempRawImage_CleanUp)();

	//misc stuff
	int		(*MarkFragments)( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );

	//model stuff
	void	(*LerpTag)( orientation_t *tag,  qhandle_t model, int startFrame, int endFrame,
					 float frac, const char *tagName );
	void	(*ModelBounds)( qhandle_t model, vec3_t mins, vec3_t maxs );

	void	(*GetLightStyle)(int style, color4ub_t color);
	void	(*SetLightStyle)(int style, int color);

	void	(*GetBModelVerts)( int bmodelIndex, vec3_t *vec, vec3_t normal );
	void	(*WorldEffectCommand)(const char *command);


	int		(*RegisterFont)(const char *name);
	int		(*_Font_HeightPixels)(const int index, const float scale);
	int		Font_HeightPixels(const int index, const float scale = 1.0f)
	{
		return (*_Font_HeightPixels)(index, scale);
	}
	int		(*_Font_StrLenPixels)(const char *s, const int index, const float scale);
	int		Font_StrLenPixels(const char *s, const int index, const float scale = 1.0f)
	{
		return (*_Font_StrLenPixels)(s, index, scale);
	}
	int		(*Font_StrLenChars) (const char *s);
	void	(*_Font_DrawString)(int x, int y, const char *s, const float *rgba, const int iFontHandle, int iMaxPixelWidth, const float scale);
	void	Font_DrawString(int x, int y, const char *s, const float *rgba, const int iFontHandle, int iMaxPixelWidth, const float scale = 1.0f)
	{
		return (*_Font_DrawString)(x, y, s, rgba, iFontHandle, iMaxPixelWidth, scale);
	}
	qboolean (*Language_IsAsian) (void);
	qboolean (*Language_UsesSpaces) (void);
	unsigned int (*AnyLanguage_ReadCharFromString)( const char **ppsText, qboolean *pbIsTrailingPunctuation /* = NULL */);
    int     (*RegisterMedia_GetLevel)(void);
    void    (*ModelInit)( void );
    void    (*InitImages)( void );
    void    (*InitShaders)( void );

    //Ghoul 2
    void    (*G2API_SetTime)(int currentTime,int clock);
    void    (*G2API_SetGhoul2ModelIndexes)(CGhoul2Info_v &ghoul2, qhandle_t *modelList, qhandle_t *skinList);
    qboolean (*G2API_HaveWeGhoul2Models)(CGhoul2Info_v &ghoul2);
    void    (*G2API_ListSurfaces)(CGhoul2Info *ghlInfo);
    void    (*G2API_ListBones)(CGhoul2Info *ghlInfo, int frame);
    int     (*G2API_AddBolt)(CGhoul2Info *ghlInfo, const char *boneName);
    qboolean (*G2API_AttachEnt)(int *boltInfo, CGhoul2Info *ghlInfoTo, int toBoltIndex, int entNum, int toModelNum);
    qboolean (*G2API_AttachG2Model)(CGhoul2Info *ghlInfo, CGhoul2Info *ghlInfoTo, int toBoltIndex, int toModel);
    void    (*G2API_CollisionDetect)(CCollisionRecord *collRecMap, CGhoul2Info_v &ghoul2, const vec3_t angles, const vec3_t position,
							int AframeNumber, int entNum, vec3_t rayStart, vec3_t rayEnd, vec3_t scale, CMiniHeap *,
							EG2_Collision eG2TraceType, int useLod, float fRadius);
    void    (*G2API_DetachEnt)(int *boltInfo);
    qboolean (*G2API_DetachG2Model)(CGhoul2Info *ghlInfo);
    qboolean (*G2API_GetAnimFileName)(CGhoul2Info *ghlInfo, char **filename);
    qboolean (*G2API_GetBoltMatrix)(CGhoul2Info_v &ghoul2, const int modelIndex, const int boltIndex, mdxaBone_t *matrix, const vec3_t angles,
							 const vec3_t position, const int AframeNum, qhandle_t *modelList, const vec3_t scale );
    qboolean (*G2API_GetBoneAnim)(CGhoul2Info *ghlInfo, const char *boneName, const int AcurrentTime, float *currentFrame,
						   int *startFrame, int *endFrame, int *flags, float *animSpeed, int *);
    qboolean (*G2API_GetBoneAnimIndex)(CGhoul2Info *ghlInfo, const int iBoneIndex, const int AcurrentTime, float *currentFrame,
						   int *startFrame, int *endFrame, int *flags, float *animSpeed, int *);
    int     (*G2API_AddSurface)(CGhoul2Info *ghlInfo, int surfaceNumber, int polyNumber, float BarycentricI, float BarycentricJ, int lod );
    int     (*G2API_InitGhoul2Model)(CGhoul2Info_v &ghoul2, const char *fileName, int, qhandle_t customSkin,
						  qhandle_t customShader, int modelFlags, int lodBias);
    qboolean (*G2API_IsPaused)(CGhoul2Info *ghlInfo, const char *boneName);
    qboolean (*G2API_PauseBoneAnim)(CGhoul2Info *ghlInfo, const char *boneName, const int AcurrentTime);
    qboolean (*G2API_PauseBoneAnimIndex)(CGhoul2Info *ghlInfo, const int boneIndex, const int AcurrentTime);
    qhandle_t (*G2API_PrecacheGhoul2Model)(const char *fileName);
    qboolean (*G2API_RemoveBolt)(CGhoul2Info *ghlInfo, const int index);
    qboolean (*G2API_RemoveBone)(CGhoul2Info *ghlInfo, const char *boneName);
    qboolean (*G2API_RemoveGhoul2Model)(CGhoul2Info_v &ghlInfo, const int modelIndex);
    qboolean (*G2API_SetBoneAngles)(CGhoul2Info *ghlInfo, const char *boneName, const vec3_t angles, const int flags,
							 const Eorientations up, const Eorientations left, const Eorientations forward,
							 qhandle_t *, int blendTime, int AcurrentTime );
    qboolean (*G2API_SetBoneAnglesMatrix)(CGhoul2Info *ghlInfo, const char *boneName, const mdxaBone_t &matrix,
								   const int flags, qhandle_t *modelList, int blendTime, int AcurrentTime);
    qboolean (*G2API_SetBoneAnim)(CGhoul2Info *ghlInfo, const char *boneName, const int startFrame, const int endFrame, const int flags, const float animSpeed, const int AcurrentTime, const float setFrame, const int blendTime);
    qboolean (*G2API_SetLodBias)(CGhoul2Info *ghlInfo, int lodBias);
    qboolean (*G2API_SetRootSurface)(CGhoul2Info_v &ghlInfo, const int modelIndex, const char *surfaceName);
    qboolean (*G2API_SetShader)(CGhoul2Info *ghlInfo, qhandle_t customShader);
    qboolean (*G2API_SetSkin)(CGhoul2Info *ghlInfo, qhandle_t customSkin);
    qboolean (*G2API_SetSurfaceOnOff)(CGhoul2Info *ghlInfo, const char *surfaceName, const int flags);
    qboolean (*G2API_StopBoneAngles)(CGhoul2Info *ghlInfo, const char *boneName);
    qboolean (*G2API_StopBoneAnim)(CGhoul2Info *ghlInfo, const char *boneName);
    qboolean (*G2API_SetGhoul2ModelFlags)(CGhoul2Info *ghlInfo, const int flags);
    int     (*G2API_AddBoltSurfNum)(CGhoul2Info *ghlInfo, const int surfIndex);
    qboolean (*G2API_RemoveSurface)(CGhoul2Info *ghlInfo, const int index);
    qboolean (*G2API_GetAnimRange)(CGhoul2Info *ghlInfo, const char *boneName,	int *startFrame, int *endFrame);
    qboolean (*G2API_GetAnimRangeIndex)(CGhoul2Info *ghlInfo, const int boneIndex, int *startFrame, int *endFrame);
    void    (*G2API_GiveMeVectorFromMatrix)(mdxaBone_t &boltMatrix, Eorientations flags, vec3_t &vec);
    void    (*G2API_CopyGhoul2Instance)(CGhoul2Info_v &ghoul2From, CGhoul2Info_v &ghoul2To, int modelIndex);
    int     (*G2API_GetGhoul2ModelFlags)(CGhoul2Info *ghlInfo);
    void    (*G2API_CleanGhoul2Models)(CGhoul2Info_v &ghoul2);
    IGhoul2InfoArray &(*TheGhoul2InfoArray)();
    int	    (*G2API_GetParentSurface)(CGhoul2Info *ghlInfo, const int index);
    int	    (*G2API_GetSurfaceIndex)(CGhoul2Info *ghlInfo, const char *surfaceName);
    char    *(*G2API_GetSurfaceName)(CGhoul2Info *ghlInfo, int surfNumber);
    char    *(*G2API_GetGLAName)(CGhoul2Info *ghlInfo);
    qboolean (*G2API_SetNewOrigin)(CGhoul2Info *ghlInfo, const int boltIndex);
    int		(*G2API_GetBoneIndex)(CGhoul2Info *ghlInfo, const char *boneName, qboolean bAddIfNotFound);
    qboolean (*G2API_StopBoneAnglesIndex)(CGhoul2Info *ghlInfo, const int index);
    qboolean (*G2API_StopBoneAnimIndex)(CGhoul2Info *ghlInfo, const int index);
    qboolean (*G2API_SetBoneAnglesIndex)(CGhoul2Info *ghlInfo, const int index, const vec3_t angles, const int flags,
							 const Eorientations yaw, const Eorientations pitch, const Eorientations roll,
							 qhandle_t *, int blendTime, int AcurrentTime);
    qboolean (*G2API_SetBoneAnglesMatrixIndex)(CGhoul2Info *ghlInfo, const int index, const mdxaBone_t &matrix,
								   const int flags, qhandle_t *, int blendTime, int AcurrentTime);
    qboolean (*G2API_SetBoneAnimIndex)(CGhoul2Info *ghlInfo, const int index, const int startFrame, const int endFrame, const int flags, const float animSpeed, const int AcurrentTime, const float setFrame, const int blendTime);
    qboolean (*G2API_SaveGhoul2Models)(CGhoul2Info_v &ghoul2, char **buffer, int *size);
    void    (*G2API_LoadGhoul2Models)(CGhoul2Info_v &ghoul2, char *buffer);
    void	(*G2API_FreeSaveBuffer)(char *buffer);
    void	(*G2API_LoadSaveCodeDestructGhoul2Info)(CGhoul2Info_v &ghoul2);
    char	*(*G2API_GetAnimFileNameIndex)(qhandle_t modelIndex);
    int		(*G2API_GetSurfaceRenderStatus)(CGhoul2Info *ghlInfo, const char *surfaceName);
    void    (*ClearStuffToStopGhoul2CrashingThings)(void);

#ifdef _NPATCH
	void    (*NPatchLevel) ( int level );
#endif // _NPATCH

} refexport_t;

//
// these are the functions imported by the refresh module
//
typedef struct {
	// print message on the local console
	void	(QDECL *Printf)( int printLevel, const char *fmt, ...);

	// abort the game
	void	(QDECL *Error)( int errorLevel, const char *fmt, ...);

	// milliseconds should only be used for profiling, never
	// for anything game related.  Get time from the refdef
	int		(*Milliseconds)( void );
	float (*flrand)(float min, float max);

	// stack based memory allocation for per-level things that
	// won't be freed
	void	(*Hunk_Clear)( void );
	void	*(*Hunk_Alloc)( int size, qboolean bZeroIt);

	// dynamic memory allocator for things that need to be freed
	void	*(*Malloc)( int iSize, memtag_t eTag, qboolean bZeroIt );
	void	(*Free)( void *buf );

	cvar_t	*(*Cvar_Get)( const char *name, const char *value, int flags );
	void	(*Cvar_Set)( const char *name, const char *value );

	void	(*Cmd_AddCommand)( const char *name, void(*cmd)(void) );
	void	(*Cmd_RemoveCommand)( const char *name );
	void	(*ArgsBuffer)( char *buffer, int bufferLength );

	int		(*Cmd_Argc) (void);
	char	*(*Cmd_Argv) (int i);

	void	(*Cmd_ExecuteText) (int exec_when, const char *text);

	// visualization for debugging collision detection
	void	(*CM_DrawDebugSurface)( void (*drawPoly)(int color, int numPoints, float *points) );

	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existance
	int		(*FS_FileIsInPAK)( const char *name );
	int		(*FS_ReadFile)( const char *name, void **buf );
	void	(*FS_FreeFile)( void *buf );
	char **	(*FS_ListFiles)( const char *name, const char *extension, int *numfilesfound );
	void	(*FS_FreeFileList)( char **filelist );
	void	(*FS_WriteFile)( const char *qpath, const void *buffer, int size );
	int		(*CM_PointContents)( const vec3_t p, clipHandle_t model );

	// cinematic stuff...
	//
	void	(*CIN_UploadCinematic)(int handle);
	int		(*CIN_PlayCinematic)( const char *arg0, int xpos, int ypos, int width, int height, int iBits, const char *psAudioFile);
	e_status (*CIN_RunCinematic) (int handle);

} refimport_t;


// this is the only function actually exported at the linker level
// If the module can't init to a valid rendering state, NULL will be
// returned.
#ifdef USE_RENDERER_DLOPEN
typedef	refexport_t* (QDECL *GetRefAPI_t) (int apiVersion, refimport_t * rimp);
#else
refexport_t*GetRefAPI( int apiVersion, refimport_t *rimp );
#endif


#endif	// __TR_PUBLIC_H

