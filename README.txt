	╔══════════════════════════════════════════╗
	║▓▒▒▒▓▒▒▒░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░║
	║░▓▒░░▓▒░░░░░░░░░░░░░░░▓▒░░░░░░░░░░░░░░░░░░║
	║░▓▒░░▓▒░░░░░░░░░░░░░░░▓▒░░░░░░░░░░░░░░░░░░║
	║░▓▒░░▓▒▓▒▒▒▒▒░▓▒▒▒▓▒▒▓▒▒▒▒░░▓▒▒▒▒▒▓▒▒▒▓▒▒▒║
	║░░▓▒▓▒▓▒░░░░▓▒░░▓▒▒░▓▒▓▒░░░▓▒░░░░▓▒▓▒░░▓▒░║
	║░░▓▒▓▒▓▒░░░░▓▒░░▓▒░░░░▓▒░░░▓▒▒▒▒▒▒▒░▓▒▒▒░░║
	║░░▓▒▓▒▓▒░░░░▓▒░░▓▒░░░░▓▒░░░▓▒░░░░░░░▓▒▒▒░░║
	║░░░▓▒░▓▒░░░░▓▒░░▓▒░░░░▓▒░▓▒▓▒░░░░▓▒▓▒░░▓▒░║
	║░░░▓▒░░▓▒▒▒▒▒░▓▒▒▒▒▒░░░▓▒▒░░▓▒▒▒▒▒▓▒▒▒▓▒▒▒║
	║░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░║
	║▓▒▒▒▓▒▒▒░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░║
	║░▓▒░░▓▒░░░░░░░░░░░░░░░▓▒░░░░░░░░░░░░░░░░░░║
	║░▓▒░░▓▒░░░░░░░░░░░░░░░▓▒░░░░░░░░░░░░░░░░░░║
	║░▓▒░░▓▒▓▒▒▒▒▒░▓▒▒▒▓▒▒▓▒▒▒▒░░▓▒▒▒▒▒▓▒▒▒▓▒▒▒║
	║░░▓▒▓▒▓▒░░░░▓▒░░▓▒▒░▓▒▓▒░░░▓▒░░░░▓▒▓▒░░▓▒░║
	║░░▓▒▓▒▓▒▒▒▒▒▒▒░░▓▒░░░░▓▒░░░▓▒▒▒▒▒▒▒░▓▒▒▒░░║
	║░░▓▒▓▒▓▒░░░░░░░░▓▒░░░░▓▒░░░▓▒░░░░░░░▓▒▒▒░░║
	║░░░▓▒░▓▒░░░░▓▒░░▓▒░░░░▓▒░▓▒▓▒░░░░▓▒▓▒░░▓▒░║
	║░░░▓▒░░▓▒▒▒▒▒░▓▒▒▒▒▒░░░▓▒▒░░▓▒▒▒▒▒▓▒▒▒▓▒▒▒║
	╠══════════════════════════════════════════╣
	╠════════v0.1 - Eris Tricker - 2018════════╣
	╠═══Based on code by Dr. Vitaliy Kurlin════╣
	╚══════════════════════════════════════════╝
This program is designed to extract, and simplify, the topological skeletons of
microscope images of domains in ferroelectrics, in order to measure vorticity in 
a given substance under certain conditions. This Readme file is intended to give a
general overview of the functionality of the program, including key processing steps,
important concepts for understanding these steps, issues/limitations with the program 
affecting performance and quality of the output, and potential solutions to these issues
I did not have the time/coding prowess to explore during this project. 

*************************************************************************************
*IMPORTANT: IF COMPILING FROM SOURCE, BOTH OPENCV AND BOOST.GRAPH LIBRARIES MUST    *
*BE INSTALLED *AND* BUILT FOR THE PROGRAM TO COMPILE. BOOST 1.65.1 AND OPENCV 3.3.1 *
*WERE USED WITH THIS SOURCE, OTHER VERSIONS MAY CAUSE TROUBLE. IF USING VISUAL      *
*STUDIO WITH THE MSVC COMPILER, THE PROGRAM MUST BE BUILT IN 'RELEASE' MODE USING   *
*FULL COMPILER OPTIMISATION OR IT WILL BE UNUSEABLY SLOW! (BECAUSE OF BOOST) 	    *
*************************************************************************************

The program operates through loading a series of images (with the same name base)
into OpenCV mat objects, these objects are then processed in instances of the 
imgprocessor class. For a given image, this Mat is simplified through the use 
of cv::adaptive_threshold alongside median blur and dilation/erosion iterations in
order to produce a noise-free binary image containg the structure information of 
the original image. 

Two functions, Remove_small_components and Remove_external_pixels are then used
to remove small regions from this binary image, as well as start to reduce the
thickness of the lines in this image, straightening them out. Remove small components
operates on both foreground and background pixels, specified by the object argument.

*************************************************************************************
*N.B: The remove_small components and remove_External pixels add significant time to*
*the execution of the program, with just two iterations of remove small components  *
*taking around 20 seconds to execute for an image of resolution 2584x1934 (average  *
*processing time for the whole image of this time is ~24s), however it is required  *
*in order to produce clean skeletons through thinning, as using the binary image    *
*alone results in a "hairy" skeleton with many branches not representative of the   *
*topological structure in the original image. As well as Remove_small_components    *
*being very time-expensive it often results in quite significant distortions of the *
*geometrical structure of the input image, making it harder to check the goodness of*
*the skeleton. A potential alternative to acheive the same results may be to use the*
*openCV contours function, which also is aware of the nesting level of the contours *
*(at present we only analyse level-1 features) however, contours provides too good  *
*of an approximation of the shapes of the curve, resulting in hairy skeletons. If a *
*straighter approximation could be used, however, we would expect overall times of  *
*completion of around ~6s for the resolution, a factor of 4 performance increase.   *
*************************************************************************************

Once the skeleton has been extracted with the Gou-Hall thinning algorithm we transfer 
it into a Boost.Graph adjacency_list which has a vertex representing each foreground 
pixel, with the vertex property set to be a cv::Point, edges are added between vertices 
depending on the relative connectivity of one foreground pixel to another, using an 
8-connected scheme (i.e. connection via diagonals is allowed). (See code comments for 
further detail). 
-------------------------------------------------------------------------------------
X=foreground pixel, 		Adjacency list (8-connected):
*=background pixel			1->0,a 
					0->1,4,6
      	 a	 			4->0,6,b
1 2 3	   X * * 	 		6->4,0,c    
8 0 4	   * X X b			
7 6 5	   * X *			
	     c	 		
-------------------------------------------------------------------------------------

Due to imperfections in the skeleton and the use of 8-connectivity, within the graph
we end up with high-order points for a cluster of (3~5) pixels around the branching 
region. This is because with 8-connectivity wherever a right angle is present in an 
arrangement of pixels, (ie 0, 1, 4, 6 are foreground), and 0 is our ideal branching 
point. Extra unwanted edges will be added diagonally between 4 and 6, causing them to 
have a higher degree than ideal. In the example above, we want 0 to have degree 3, as
it is the branch point of 3 distinct lines (a,b,c) however because 4 and 6 have a mutual 
diagonal connection, when the connections to lines b and c are made, they will have 
an order of 3 as well. This makes it more difficult to identify the branching points
in the graph representation, and introduces additional cycles to the graph, making the
topological properties harder to analyse. However, 4 connectivity can't be used as 1 
needs to connect to both 0 and line a diagonally.

*************************************************************************************
*This problem will be resolved by use of a  better skeletonisation algorithm, or a  *
*combination of algorithms. Gou-Hall works very well for line sections but a further*
*stage is needed to correctly thin the branch sections.				    *
*************************************************************************************

In order to bypass this issue of excess edges in the skeleton graph, further processing
steps are needed. At present this is acheived by scanning all vertices in the skeleton
graph and extracting those with a degree greater than 2 (as all vertices in a simple
line are guaranteed to have a degree of 2 through Guo-Hall thinning). These non-trivial
vertices are then copied into a graph of their own, and connected according to the 
connections in the skeleton graph (i.e. adjacent pixels to a given pixel are searched,
and those points also to have degree>2 are connected with an edge). This results in a graph
containg just the important clusters of branching points, which can be analysed using
the Boost.Graph connected_components algorithm. From the connected components result
a Vector of vertexCluster object is filled, with the vertexCluster class containing all
the points which constitute a given vertex point cluster. The class also posesses a 
member function which extracts a centre pixel to be considered as the branch point:
if one vertex has higher order than all others, or there is just one vertex, then this
is chosen, otherwise the average of all the constituent points is calculated, and these
centers are stored in a vector of points, branchPoints. 

*************************************************************************************
*If the skeleton was ideal, the use of this class would be unecessary, as all points*
*of degree>2 could be directly extracted from the skeleton graph, saving computation*
*time as well as memory. 							    *
*************************************************************************************

'Edge' extraction follows a very similar principle to the 'vertex' extraction, though
there are a few additional complications. Firstly, all vertices with degree=2 are copied
to a new graph, and edges are added between points based on looking at a particular points
neighbours, and adding edges to these neighbours if they are also an 'edge' point. As
with the vertex clusters, connected components is then used to extract the indivual point
clusters making up the edges, and these points are stored in an edgeCluster object.
Within the edgeCluster class the end points of the edge (those points with degree 1 with
respect to the edge clusters graph) are stored, and the rest of the constituent points 
for the edge sorted (as the order in which points are extracted from the main graph does
not conserve the order in which the points are connected). At this point the simplest
topological graph can be reconstructed by adding a vertex for each of the calculated
branching centre points, then looking at the end points of each of the edges and adding an 
edge between the two branch points closest to the end points of a given edge cluster.

*************************************************************************************
*An ideal skeleton would also help with this reconstruction process, as it would    *
*ensure that edge end points would always be within 1 pixel of the branch point.    *
*Currently, the reconstruction accuracy can sometimes be affected, especially if the*
*end point lies an equal distance from two branching points. Additionally, the      *
*process of searching for the nearest branch point to an end is expensive, due to   *
*the fact potentially every branch point needsto be checked against every end point,*
*and the distance calculation uses expensive arithmetic functions such as sqrt.     *
*In terms of alternative approaches to this problem, Boost's subgraph and filtered  *
*graph functionality could be used to extract these features, however these 	    *
*features often require 'roll your own' structs to handle the process which are	    *
*more abstract and far more complicated to write and debug than the currently used  *
*clunky-but-simple approach. If execution speed becomes more of a priority however, * 
*these methods using boost's function-like-macro algorithms may provide good gains  *
*in performance, especially if the alogrithms are able to be muiltithreaded.        *
*************************************************************************************

This simplest possible graph produced is plotted in the printEdges function, drawing
lines for every edge in the graph, then circles for the vertices. However, whilst this
simplest graph preserves most of the key topological features, geometric detail is lost, 
such as any small cycle or any parallel edges between two branch points. Therefore, the
edgeCluster class contains a member function getImportantPoints(), which calculates
which vertices should be conserved. The function first calculates the perpendicular 
distance from each non-end point to the line described by the end points. This set of 
distance values then has its mean and standard deviation calculated. Points are then
chosen for conservation based upon their distance from the line being above a threshold
determined as mean+sigma*stdDev, where sigma is a variable passed to the function.
At present a sigma value of 1 seems to work well, meaning only points at least 1 standard
deviation away from the straight line are conserved . In addition to this statistical
selection, another variable, resolution can be varied, this determines how many points
are skipped between points when testing for conservation, such that the graph is still
simplified and not full of redundant degree-2 vertices; 20 is used at present.

This simpler graph is somewhat harder to reconstruct owing to the additional edges:
first the relevant branch vertex closest to the edge end is selected, and an edge added
between this and the first conserved point, then sequentially between all the convserved
points, and finally between the last conserved point and the branch point closest to the
end point of that particular edge. 

*************************************************************************************
*Once issues with the simplified skeleton have been sorted (through improvement of  *
*the skeletonisation process, or the reconstruction process) the next feature to    *
*implement is the topological analysis of the simplified graph, which will involve  *
*mapping the graph onto a polygonal mesh, such that the number of polygons the graph*
*regions define can be charachterised, in particualr the number of even n-gons,     *
*defined by degree-3 vertices is crucial to the task of vortex analysis.	    *
*************************************************************************************

*************************************************************************************
*More Significant potential changes:						    *
*There's currently quite a few inefficienies in this process, e.g having to sort    *
*edge vertex clusters seperately, as well as storing all the vertices. If an 	    *
*algorithm can be implemented which walks through the skeleton graph storing edge   *
*vertex and edge points as it progresses (ie store an edge vertex every direction   *
*change, so the standard deviation stuff doesn't need to happen), then big gains in *
*efficiency could be made. This potential method, however, would only work easily   *
*given an ideal skeleton, as the current high-order branching clusters would make a *
*walk harder to execute.							    *
*************************************************************************************

 