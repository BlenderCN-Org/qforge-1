
#include "qedefs.h"

id	map_i;

@implementation Map

/*
===============================================================================

FILE METHODS

===============================================================================
*/

- (id) init
{
	[super init];
	map_i = self;
	minz = 0;
	maxz = 80;
	
	oldselection = [[NSMutableArray alloc] init];
	
	return self;
}

- (void) saveSelected
{
	int		i, c;
	id		o, w;
	
	[oldselection removeAllObjects];
	w = [self objectAtIndex: 0];
	c = [w count];
	sb_newowner = oldselection;
	for (i = 0; i < c; i++) {
		o = [w objectAtIndex: 0];
		if ([o selected]) {
			[o moveToEntity];
		} else {
			[w removeObjectAtIndex: 0];
			[o release];
		}
	}
	
	c = [self count];
	for (i = 0; i < c; i++) {
		o = [self objectAtIndex: 0];
		[self removeObjectAtIndex: 0];
		[o removeAllObjects];
		[o release];
	}

	return;
}

- (void) addSelected
{
	int		i, c;
	id		n, w;
	
	c = [oldselection count];
	w = [self objectAtIndex: 0];	// world object

	sb_newowner = w;
	for (i = 0; i < c; i++) {
		n = [oldselection objectAtIndex: i--];
		[n moveToEntity];
		c--;
	}
	[oldselection removeAllObjects];
	
	return;
}

- (void) newMap
{
	id	ent;
	
	[self saveSelected];
	ent = [[Entity alloc] initClass: "worldspawn"];
	[self addObject: ent];
	currentEntity = NULL;
	[self setCurrentEntity: ent];
	[self addSelected];

	return;
}

- (id) currentEntity
{
	return currentEntity;
}

- (void) setCurrentEntity: ent
{
	id	old;
	
	old = currentEntity;
	currentEntity = ent;
	if (old != ent) {
		[things_i newCurrentEntity];	// update inspector
		[inspcontrol_i changeInspectorTo:i_things];
	}
	
	return;
}

- (float) currentMinZ
{
	float	grid;
	
	grid = [xyview_i gridsize];
	minz = grid * rint(minz/grid);
	return minz;
}

- (void) setCurrentMinZ: (float) m
{
	if (m > -2048)
		minz = m;
	return;
}

- (float) currentMaxZ
{
	float	grid;
	
	[self currentMinZ];	// grid align
	
	grid = [xyview_i gridsize];
	maxz = grid * rint(maxz/grid);

	if (maxz <= minz)
		maxz = minz + grid;
	return maxz;
}

- (void) setCurrentMaxZ: (float) m
{
	if (m < 2048)
		maxz = m;
	return;
}

- (void) removeObject: (id) o
{
	[super removeObject: o];
	
	if (o == currentEntity) {	// select the world
		[self setCurrentEntity: [self objectAtIndex: 0]];
	}
}

- (void) writeStats
{
	FILE			*f;
	extern int		c_updateall;
	struct timeval	tp;
	struct timezone tzp;

	gettimeofday (&tp, &tzp);
	
	f = fopen ([developerLogFile cString], "a");
	fprintf (f,"%i %i\n", (int) tp.tv_sec, c_updateall);
	c_updateall = 0;
	fclose (f);
	return;
}

- (int) numSelected
{
	int		i, c;
	int		num = 0;
	
	c = [currentEntity count];
	for (i = 0; i < c; i++)
		if ([[currentEntity objectAtIndex: i] selected])
			num++;
	return num;
}

- (id) selectedBrush
{
	int		i, c;
	
	c = [currentEntity count];
	for (i=0 ; i<c ; i++)
		if ( [[currentEntity objectAtIndex: i] selected] )
			return [currentEntity objectAtIndex: i];
	return nil;
}


/*
=================
readMapFile
=================
*/
- (void) readMapFile: (NSString *) fname
{
	char	*dat, *cl;
	id		new;
	id		ent;
	int		i, c;
	vec3_t	org;
	float	angle;
	
	[self saveSelected];
	
	NSLog (@"loading %s\n", fname);

	LoadFile ((char *) [fname cString], (void **) &dat);
	StartTokenParsing (dat);

	do {
		new = [[Entity alloc] initFromTokens];
		if (!new)
			break;
		[self addObject: new];		
	} while (1);

	free (dat);

	[self setCurrentEntity: [self objectAtIndex: 0]];

	[self addSelected];
		
	// load the apropriate texture wad
	dat = [currentEntity valueForQKey: "wad"];
	if (dat && dat[0]) {
		if (dat[0] == '/') {	// remove old style fullpaths
			[currentEntity removeKeyPair: "wad"];
		} else {
			if (strcmp ([texturepalette_i currentWad], dat) )
				[project_i 	setTextureWad: dat];
		}
	}

	// center the camera and XY view on the playerstart
	c = [self count];
	for (i=1 ; i<c ; i++) {
		ent = [self objectAtIndex: i];
		cl = [ent valueForQKey: "classname"];
		if (cl && !strcasecmp (cl,"info_player_start")) {
			angle = atof( [ent valueForQKey: "angle"] );
			angle = angle/180*M_PI;
			[ent getVector: org forKey: "origin"];
			[cameraview_i setOrigin: org angle:angle];
			[xyview_i centerOn: org];
			break;
		}
	}
	
	return;
}

/*
=================
writeMapFile
=================
*/
- (void) writeMapFile: (NSString *) fname useRegion: (BOOL) reg
{
	FILE	*f;
	int		i;
	
	NSLog (@"writeMapFile: %s", fname);
	
	if (!(f = fopen ([fname cString], "w")))
		NSLog (@"couldn't write %s", fname);

	for (i = 0; i < [self count]; i++)
		[[self objectAtIndex: i] writeToFILE: f region: reg];
			
	fclose (f);
	
	return;
}

/*
==============================================================================

DRAWING

==============================================================================
*/

- ZDrawSelf
{
	int		i, count;
	
	count = [self count];

	for (i=0 ; i<count ; i++)
		[[self objectAtIndex: i] ZDrawSelf];

	return self;
}

- RenderSelf: (void (*) (face_t *))callback
{
	int		i, count;
	
	count = [self count];

	for (i=0 ; i<count ; i++)
		[[self objectAtIndex: i] RenderSelf: callback];

	return self;
}


//============================================================================


/*
===================
entityConnect

A command-shift-click on an entity while an entity is selected will
make a target connection from the original entity.
===================
*/
- entityConnect: (vec3_t)p1 : (vec3_t)p2
{
	id	oldent, ent;
	
	oldent = [self currentEntity];
	if (oldent == [self objectAtIndex: 0])
	{
		qprintf ("Must have a non-world entity selected to connect");
		return self;
	}

	[self selectRay: p1 : p2 : YES];
	ent = [self currentEntity];
	if (ent == oldent)
	{
		qprintf ("Must click on a different entity to connect");
		return self;
	}
	
	if (ent == [self objectAtIndex: 0])
	{
		qprintf ("Must click on a non-world entity to connect");
		return self;
	}
	
	[oldent setKey:"target" toValue: [ent targetname]];
	[quakeed_i updateAll];

	return self;
}


/*
=================
selectRay

If ef is true, any entity brush along the ray will be selected in preference
to intervening world brushes
=================
*/
- selectRay: (vec3_t)p1 : (vec3_t)p2 : (BOOL)ef
{
	int		i, j, c, c2;
	id		ent, bestent;
	id		brush, bestbrush;
	int		face, bestface;
	float	time, besttime;
	texturedef_t	*td;
	
	bestent = nil;
	bestface = -1;
	bestbrush = nil;
	besttime = 99999;
	
	c = [self count];
	for (i = c - 1; i >= 0; i--) {
		ent = [self objectAtIndex: i];
		c2 = [ent count];
		for (j = 0; j < c2; j++) {
			brush = [ent objectAtIndex: j];
			[brush hitByRay: p1 : p2 : &time : &face];
			if (time < 0 || time >besttime)
				continue;
			bestent = ent;
			besttime = time;
			bestbrush = brush;
			bestface = face;
		}
		if (i == 1 && ef && bestbrush)
			break;		// found an entity, so don't check the world
	}
	
	if (besttime == 99999) {
		qprintf ("trace missed");
		return self;
	}

	if ( [bestbrush regioned] ) {
		qprintf ("WANRING: clicked on regioned brush");
		return self;
	}
	
	if (bestent != currentEntity) {
		[self makeSelectedPerform: @selector(deselect)];
		[self setCurrentEntity: bestent];
	}
	
	[quakeed_i disableFlushWindow];
	if (![bestbrush selected]) {
		if ([map_i numSelected] == 0) {	// don't grab texture if others are selected
			td = [bestbrush texturedefForFace: bestface];
			[texturepalette_i setTextureDef: td];
		}

		[bestbrush setSelected: YES];
		qprintf ("selected entity %i brush %i face %i", [self indexOfObject:bestent], [bestent indexOfObject: bestbrush], bestface);
	} else {
		[bestbrush setSelected: NO];
		qprintf ("deselected entity %i brush %i face %i", [self indexOfObject:bestent], [bestent indexOfObject: bestbrush], bestface);
	}

	[quakeed_i enableFlushWindow];
	[quakeed_i updateAll];
	
	return self;
}

/*
=================
grabRay

only checks the selected brushes
Returns the brush hit, or nil if missed.
=================
*/
- grabRay: (vec3_t)p1 : (vec3_t)p2
{
	int		i, j, c, c2;
	id		ent;
	id		brush, bestbrush;
	int		face;
	float	time, besttime;
	
	bestbrush = nil;
	besttime = 99999;
	
	c = [self count];
	for (i=0 ; i<c ; i++)
	{
		ent = [self objectAtIndex: i];		
		c2 = [ent count];
		for (j=0 ; j<c2 ; j++)
		{
			brush = [ent objectAtIndex: j];
			if (![brush selected])
				continue;
			[brush hitByRay: p1 : p2 : &time : &face];
			if (time < 0 || time >besttime)
				continue;
			besttime = time;
			bestbrush = brush;
		}
	}
	
	if (besttime == 99999)
		return nil;
	
	return bestbrush;
}

/*
=================
getTextureRay
=================
*/
- getTextureRay: (vec3_t)p1 : (vec3_t)p2
{
	int		i, j, c, c2;
	id		ent, bestent;
	id		brush, bestbrush;
	int		face, bestface;
	float	time, besttime;
	texturedef_t	*td;
	vec3_t	mins, maxs;		


	bestbrush = nil;
	bestent = nil;
	besttime = 99999;
	bestface = -1;
	c = [self count];
	for (i=0 ; i<c ; i++)
	{
		ent = [self objectAtIndex: i];		
		c2 = [ent count];
		for (j=0 ; j<c2 ; j++)
		{
			brush = [ent objectAtIndex: j];
			[brush hitByRay: p1 : p2 : &time : &face];
			if (time < 0 || time >besttime)
				continue;
			bestent = ent;
			bestface = face;
			besttime = time;
			bestbrush = brush;
		}
	}
	
	if (besttime == 99999)
		return nil;

	if ( ![bestent modifiable])
	{
		qprintf ("can't modify spawned entities");
		return self;
	}
	
	td = [bestbrush texturedefForFace: bestface];
	[texturepalette_i setTextureDef: td];
	
	qprintf ("grabbed texturedef and sizes");
	
	[bestbrush getMins: mins maxs: maxs];
	
	minz = mins[2];
	maxz = maxs[2];
	
	return bestbrush;
}

/*
=================
setTextureRay
=================
*/
- setTextureRay: (vec3_t)p1 : (vec3_t)p2 : (BOOL)allsides;
{
	int		i, j, c, c2;
	id		ent, bestent;
	id		brush, bestbrush;
	int		face, bestface;
	float	time, besttime;
	texturedef_t	td;
		
	bestent = nil;
	bestface = -1;
	bestbrush = nil;
	besttime = 99999;
	
	c = [self count];
	for (i=0 ; i<c ; i++)
	{
		ent = [self objectAtIndex: i];		
		c2 = [ent count];
		for (j=0 ; j<c2 ; j++)
		{
			brush = [ent objectAtIndex: j];
			[brush hitByRay: p1 : p2 : &time : &face];
			if (time < 0 || time >besttime)
				continue;
			bestent = ent;
			besttime = time;
			bestbrush = brush;
			bestface = face;
		}
	}
	
	if (besttime == 99999)
	{
		qprintf ("trace missed");
		return self;
	}

	if ( ![bestent modifiable])
	{
		qprintf ("can't modify spawned entities");
		return self;
	}
	
	if ( [bestbrush regioned] )
	{
		qprintf ("WANRING: clicked on regioned brush");
		return self;
	}
	
	[texturepalette_i getTextureDef: &td];
	
	[quakeed_i disableFlushWindow];
	if (allsides)
	{
		[bestbrush setTexturedef: &td];
		qprintf ("textured entity %i brush %i", [self indexOfObject:bestent], [bestent indexOfObject: bestbrush]);
	}
	else 
	{
		[bestbrush setTexturedef: &td forFace: bestface];
		qprintf ("deselected entity %i brush %i face %i", [self indexOfObject:bestent], [bestent indexOfObject: bestbrush], bestface);
	}
	[quakeed_i enableFlushWindow];
		
	[quakeed_i updateAll];
	
	return self;
}


/*
==============================================================================

OPERATIONS ON SELECTIONS

==============================================================================
*/

- makeSelectedPerform: (SEL)sel
{
	int	i,j, c, c2;
	id	ent, brush;
	int	total;
	
	total = 0;
	c = [self count];
	for (i=c-1 ; i>=0 ; i--)
	{
		ent = [self objectAtIndex: i];
		c2 = [ent count];
		for (j = c2-1 ; j >=0 ; j--)
		{
			brush = [ent objectAtIndex: j];
			if (! [brush selected] )
				continue;
			if ([brush regioned])
				continue;
			total++;
			[brush perform:sel];
		}
	}

//	if (!total)
//		qprintf ("nothing selected");
		
	return self;	
}

- makeUnselectedPerform: (SEL)sel
{
	int	i,j, c, c2;
	id	ent, brush;
	
	c = [self count];
	for (i=c-1 ; i>=0 ; i--)
	{
		ent = [self objectAtIndex: i];
		c2 = [ent count];
		for (j = c2-1 ; j >=0 ; j--)
		{
			brush = [ent objectAtIndex: j];
			if ( [brush selected] )
				continue;
			if ([brush regioned])
				continue;
			[brush perform:sel];
		}
	}

	return self;	
}

- makeAllPerform: (SEL)sel
{
	int	i,j, c, c2;
	id	ent, brush;
	
	c = [self count];
	for (i=c-1 ; i>=0 ; i--)
	{
		ent = [self objectAtIndex: i];
		c2 = [ent count];
		for (j = c2-1 ; j >=0 ; j--)
		{
			brush = [ent objectAtIndex: j];
			if ([brush regioned])
				continue;
			[brush perform:sel];
		}
	}

	return self;	
}

- makeGlobalPerform: (SEL)sel	// in and out of region
{
	int	i,j, c, c2;
	id	ent, brush;
	
	c = [self count];
	for (i=c-1 ; i>=0 ; i--)
	{
		ent = [self objectAtIndex: i];
		c2 = [ent count];
		for (j = c2-1 ; j >=0 ; j--)
		{
			brush = [ent objectAtIndex: j];
			[brush perform:sel];
		}
	}

	return self;	
}


void sel_identity (void)
{
	sel_x[0]=1; sel_x[1]=0; sel_x[2]=0;
	sel_y[0]=0; sel_y[1]=1; sel_y[2]=0;
	sel_z[0]=0; sel_z[1]=0; sel_z[2]=1;
}

- transformSelection
{
	if ( ![currentEntity modifiable])
	{
		qprintf ("can't modify spawned entities");
		return self;
	}

// find an origin to apply the transformation to
	sb_mins[0] = sb_mins[1] = sb_mins[2] = 99999;
	sb_maxs[0] = sb_maxs[1] = sb_maxs[2] = -99999;
	[self makeSelectedPerform: @selector(addToBBox)];
	sel_org[0] = [xyview_i snapToGrid: (sb_mins[0] + sb_maxs[0])/2];
	sel_org[1] = [xyview_i snapToGrid: (sb_mins[1] + sb_maxs[1])/2];
	sel_org[2] = [xyview_i snapToGrid: (sb_mins[2] + sb_maxs[2])/2];
	
// do it!
	[self makeSelectedPerform: @selector(transform)];

	[quakeed_i updateAll];
	return self;
}


void swapvectors (vec3_t a, vec3_t b)
{
	vec3_t	temp;
	
	VectorCopy (a, temp);
	VectorCopy (b, a);
	VectorSubtract (vec3_origin, temp, b);
}

/*
===============================================================================

UI operations

===============================================================================
*/

- rotate_x: sender
{
	sel_identity ();
	swapvectors(sel_y, sel_z);
	[self transformSelection];
	return self;
}

- rotate_y: sender
{
	sel_identity ();
	swapvectors(sel_x, sel_z);
	[self transformSelection];
	return self;
}

- rotate_z: sender
{
	sel_identity ();
	swapvectors(sel_x, sel_y);
	[self transformSelection];
	return self;
}


- flip_x: sender
{
	sel_identity ();
	sel_x[0] = -1;
	[self transformSelection];
	[map_i makeSelectedPerform: @selector(flipNormals)];
	return self;
}

- flip_y: sender
{
	sel_identity ();
	sel_y[1] = -1;
	[self transformSelection];
	[map_i makeSelectedPerform: @selector(flipNormals)];
	return self;
}


- flip_z: sender
{
	sel_identity ();
	sel_z[2] = -1;
	[self transformSelection];
	[map_i makeSelectedPerform: @selector(flipNormals)];
	return self;
}


- cloneSelection: sender
{
	int		i,j , c, originalElements;
	id		o, b;
	id		new;
	
	sb_translate[0] = sb_translate[1] = [xyview_i gridsize];
	sb_translate[2] = 0;

// copy individual brushes in the world entity
	o = [self objectAtIndex: 0];
	c = [o count];
	for (i=0 ; i<c ; i++)
	{
		b = [o objectAtIndex: i];
		if (![b selected])
			continue;
			
	// copy the brush, then translate the original
		new = [b copy];
		[new setSelected: YES];
		[new translate];
		[b setSelected: NO];
		[o addObject: new];
	}
	
// copy entire entities otherwise
	originalElements = [self count];	// don't copy the new ones
	for (i=1 ; i<originalElements ; i++)
	{
		o = [self objectAtIndex: i];
		if (![[o objectAtIndex: 0] selected])
			continue;

		new = [o copy];
		[self addObject: new];

		c = [o count];
		for (j=0 ; j<c ; j++)
			[[o objectAtIndex: j] setSelected: NO];
		
		c = [new count];
		for (j=0 ; j<c ; j++)
		{
			b = [new objectAtIndex: j];
			[b translate];
			[b setSelected: YES];
		}
	}

	[quakeed_i updateAll];

	return self;
}


- selectCompleteEntity: sender
{
	id	o;
	int	i, c;
	
	o = [self selectedBrush];
	if (!o)
	{
		qprintf ("nothing selected");
		return self;
	}
	o = [o parent];
	c = [o count];
	for (i=0 ; i<c ; i++)
		[[o objectAtIndex: i] setSelected: YES];	
	qprintf ("%i brushes selected", c);

	[quakeed_i updateAll];

	return self;
}

- makeEntity: sender
{
	if (currentEntity != [self objectAtIndex: 0])
	{
		qprintf ("ERROR: can't makeEntity inside an entity");
		NSBeep ();
		return self;
	}
	
	if ( [self numSelected] == 0)
	{
		qprintf ("ERROR: must have a seed brush to make an entity");
		NSBeep ();
		return self;
	}
	
	sb_newowner = [[Entity alloc] initClass: [things_i spawnName]];

	if ( [sb_newowner modifiable] )
		[self makeSelectedPerform: @selector(moveToEntity)];
	else
	{	// throw out seed brush and select entity fixed brush
		[self makeSelectedPerform: @selector(remove)];
		[[sb_newowner objectAtIndex: 0] setSelected: YES];
	}
	
	[self addObject: sb_newowner];
	[self setCurrentEntity: sb_newowner];
	
	[quakeed_i updateAll];
	
	return self;
}


- selbox: (SEL)selector
{
	id	b;
	
	if ([self numSelected] != 1)
	{
		qprintf ("must have a single brush selected");
		return self;
	} 

	b = [self selectedBrush];
	[b getMins: select_min maxs: select_max];
	[b remove];
	
	[self makeUnselectedPerform: selector];
	
	qprintf ("identified contents");
	[quakeed_i updateAll];
	
	return self;
}

- selectCompletelyInside: sender
{
	return [self selbox:  @selector(selectComplete)];
}

- selectPartiallyInside: sender
{
	return [self selbox:  @selector(selectPartial)];
}


- tallBrush: sender
{
	id		b;
	vec3_t	mins, maxs;
	texturedef_t	td;
	
	if ([self numSelected] != 1)
	{
		qprintf ("must have a single brush selected");
		return self;
	} 

	b = [self selectedBrush];
	td = *[b texturedef];
	[b getMins: mins maxs: maxs];
	[b remove];

	mins[2] = -2048;
	maxs[2] = 2048;
	
	b = [[SetBrush alloc] initOwner: [map_i objectAtIndex:0] mins: mins maxs: maxs texture: &td];
	[[map_i objectAtIndex: 0] addObject: b];
	[b setSelected: YES];
	[quakeed_i updateAll];
		
	return self;
}

- shortBrush: sender
{
	id		b;
	vec3_t	mins, maxs;
	texturedef_t	td;
	
	if ([self numSelected] != 1)
	{
		qprintf ("must have a single brush selected");
		return self;
	} 

	b = [self selectedBrush];
	td = *[b texturedef];
	[b getMins: mins maxs: maxs];
	[b remove];

	mins[2] = 0;
	maxs[2] = 16;
	
	b = [[SetBrush alloc] initOwner: [map_i objectAtIndex:0] mins: mins maxs: maxs texture: &td];
	[[map_i objectAtIndex: 0] addObject: b];
	[b setSelected: YES];
	[quakeed_i updateAll];
	
	return self;
}

/*
==================
subtractSelection
==================
*/
- subtractSelection: semder
{
	int		i, j, c, c2;
	id		o, o2;
	id		sellist, sourcelist;
	
	qprintf ("performing brush subtraction...");

	sourcelist = [[NSMutableArray alloc] init];
	sellist = [[NSMutableArray alloc] init];
	carve_in = [[NSMutableArray alloc] init];
	carve_out = [[NSMutableArray alloc] init];
	
	c = [currentEntity count];
	for (i=0 ; i<c ; i++)
	{
		o = [currentEntity objectAtIndex: i];
		if ([o selected])
			[sellist addObject: o];
		else
			[sourcelist addObject: o];
	}
	
		
	c = [sellist count];
	for (i=0 ; i<c ; i++)
	{
		o = [sellist objectAtIndex: i];
		[o setCarveVars];
		
		c2 = [sourcelist count];
		for (j=0 ; j<c2 ; j++)
		{
			o2 = [sourcelist objectAtIndex: j];
			[o2 carve];
			[carve_in removeAllObjects];
		}

		[sourcelist free];	// the individual have been moved/freed
		sourcelist = carve_out;
		carve_out = [[NSMutableArray alloc] init];
	}

// add the selection back to the remnants
	[currentEntity removeAllObjects];
	[currentEntity addObjectsFromArray: sourcelist];
	[currentEntity addObjectsFromArray: sellist];

	[sourcelist free];
	[sellist free];
	[carve_in free];
	[carve_out free];
	
	if (![currentEntity count])
	{
		o = currentEntity;
		[self removeObject: o];
		[o free];
	}

	qprintf ("subtracted selection");
	[quakeed_i updateAll];
	
	return self;
}


@end
