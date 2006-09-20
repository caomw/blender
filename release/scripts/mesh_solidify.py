#!BPY
"""
Name: 'Solidify Mesh'
Blender: 240
Group: 'Mesh'
Tooltip: 'Makes the mesh solid by creating a second skin of a set width.'
"""

__author__ = "Campbell Barton"
__url__ = ("www.blender.org", "blenderartists.org")
__version__ = "1.0"

__bpydoc__ = """\
This script makes a skin from the selected faces.
Optionaly you can skin between the original and new faces to make a watertight solid object
"""


from Blender import *
import BPyMesh
Ang= Mathutils.AngleBetweenVecs
SMALL_NUM=0.00001


# returns a length from an angle
# Imaging a 2d space.
# there is a hoz line at Y1 going to inf on both X ends, never moves (LINEA)
# down at Y0 is a unit length line point up at (angle) from X0,Y0 (LINEB)
# This function returns the length of LINEB at the point it would intersect LINEA
# - Use this for working out how long to make the vector - differencing it from surrounding faces,
import math

def lengthFromAngle(angle):
	if angle < SMALL_NUM:
		return 1.0
	angle = 2*math.pi*angle/360
	x,y = math.cos(angle), math.sin(angle)
	# print "YX", x,y
	# 0 d is hoz to the right.
	# 90d is vert upward.
	fac=1/x
	x=x*fac
	y=y*fac
	return math.sqrt((x*x)+(y*y))


def main():
	scn = Scene.GetCurrent()
	ob = scn.getActiveObject()
	if not ob or ob.getType() != 'Mesh':
		Draw.PupMenu('ERROR: Active object is not a mesh, aborting.')
		return
	is_editmode = Window.EditMode() 
	if is_editmode:
		Window.EditMode(0)
	
	
	# Create the variables.
	PREF_THICK = Draw.Create(-0.1)
	PREF_SKIN_SIDES= Draw.Create(1)
	PREF_REM_ORIG= Draw.Create(0)
	
	pup_block = [\
	'Projection',\
	('thick:', PREF_THICK, -10, 10, 'lower for more projection groups, higher for less distortion.'),\
	('Skin Sides', PREF_SKIN_SIDES, 'skin between the original and new faces.'),\
	('Remove Original', PREF_REM_ORIG, 'Remove the original faces used for skinning.'),\
	]
	
	if not Draw.PupBlock('Skin Selected Faces', pup_block):
		return
	
	PREF_THICK= PREF_THICK.val
	PREF_SKIN_SIDES= PREF_SKIN_SIDES.val
	PREF_REM_ORIG= PREF_REM_ORIG.val
	
	
	
	Window.WaitCursor(1)
	
	# Main code function	
	me = ob.getData(mesh=1)
	
	faces_sel= [f for f in me.faces if f.sel]
	
	BPyMesh.meshCalcNormals(me)
	normals= [v.no for v in me.verts]
	vertFaces= [[] for i in xrange(len(me.verts))]
	for f in me.faces:
		no=f.no
		for v in f:
			vertFaces[v.index].append(no)
	
	# Scale the normals by the face angles from the vertex Normals.
	for i in xrange(len(me.verts)):
		length=0.0
		if vertFaces[i]:
			for fno in vertFaces[i]:
				try:
					a= Ang(fno, normals[i])
				except:
					a= 0	
				if a>=90:
					length+=1
				elif a < SMALL_NUM:
					length+= 1
				else:
					length+= lengthFromAngle(a)
			
			length= length/len(vertFaces[i])
			#print 'LENGTH %.6f' % length
			normals[i]= (normals[i] * length) * PREF_THICK
	
	len_verts = len( me.verts )
	len_faces = len( me.faces )
	
	vert_mapping= [-1] * len(me.verts)
	verts= []
	for f in faces_sel:
		for v in f:
			i= v.index
			if vert_mapping[i]==-1:
				vert_mapping[i]= len_verts + len(verts)
				verts.append(v.co + normals[v.index])
	
	#verts= [v.co + normals[v.index] for v in me.verts]
	
	me.verts.extend( verts )
	#faces= [tuple([ me.verts[v.index+len_verts] for v in reversed(f.v)]) for f in me.faces ]
	faces= []
	faces= [ tuple([vert_mapping[v.index] for v in reversed(f.v)]) for f in faces_sel ]
	me.faces.extend( faces )
	
	for i, orig_f in enumerate(faces_sel):
		new_f= me.faces[len_faces + i]
		new_f.mat = orig_f.mat
		new_f.smooth = orig_f.smooth
		orig_f.sel=False
		new_f.sel= True
		new_f = me.faces[i+len_faces]
		if me.faceUV:
			new_f.col = [c for c in reversed(orig_f.col)]
			new_f.uv = [c for c in reversed(orig_f.uv)]
			new_f.mode = orig_f.mode
			new_f.flag = orig_f.flag
			if orig_f.image:
				new_f.image = orig_f.image
	
	# Now add quads between if we wants
	
	if PREF_SKIN_SIDES:
		skin_side_faces= []
		skin_side_faces_orig= []
		# Get edges of faces that only have 1 user - so we can make walls
		edges = {}
		for f in faces_sel:
			f_v= f.v
			for i in xrange(len(f_v)):
				i1=f_v[i].index
				i2=f_v[i-1].index
				if i1>i2:	edge_key= i2, i1
				else:		edge_key= i1, i2
				
				if edges.has_key(edge_key):
					edges[edge_key]= None
				else:
					edges[edge_key] = f, f_v, i, i-1
		
		# Edges are done. extrude the single user edges.
		for edge_face_data in edges.itervalues():
			if edge_face_data: # != None
				f, f_v, i1, i2 = edge_face_data
				v1i,v2i= f_v[i1].index, f_v[i2].index
				# Now make a new Face
				skin_side_faces.append( (v1i, v2i, vert_mapping[v2i], vert_mapping[v1i]) )
				skin_side_faces_orig.append((f, len(me.faces) + len(skin_side_faces_orig), i1, i2))
		
		me.faces.extend(skin_side_faces)
		
		
		# Now assign properties.
		for i, origfData in enumerate(skin_side_faces_orig):
			orig_f, new_f_idx, i1, i2 = origfData
			new_f= me.faces[new_f_idx]
			
			new_f.mat= orig_f.mat
			new_f.smooth= orig_f.smooth
			if me.faceUV:
				new_f.mode= orig_f.mode
				new_f.flag= orig_f.flag
				if orig_f.image:
					new_f.image= orig_f.image
					
				uv1= orig_f.uv[i1]
				uv2= orig_f.uv[i2]
				col1= orig_f.col[i1]
				col2= orig_f.col[i2]
				
				new_f.uv= (uv1, uv2, uv2, uv1)
				new_f.col= (col1, col2, col2, col1)
	
	if PREF_REM_ORIG:
		me.faces.delete(0, faces_sel)
	
	
	Window.WaitCursor(0)
	if is_editmode:
		Window.EditMode(1)
	
	Window.RedrawAll()

if __name__ == '__main__':
	main()