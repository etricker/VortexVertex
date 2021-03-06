// testing.cpp : Defines the entry point for the console application.
//
//std

#include "stdafx.h"
#include <fstream>
#include <iostream>
#include <deque>
#include <set>
#include <string>
#include <iterator>
#include <utility>
#include <algorithm>
#include <numeric>
#include <vector> 
#include <queue>
#include <limits>
#include <unordered_map>


// OpenCV
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/core/traits.hpp>

#include "opencv2/core/utility.hpp"

//thinning (nicked from https://github.com/wuciawe/guo-hall-thinning)
//#define HAVE_STRUCT_TIMESPEC//it ballses up without this
//#include "thinning.h"

using namespace cv;
//MUST BE COMPILED IN RELEASE MODE, IN DEBUG BOOST MAKES THINGS FUCKING GLACIAL!!!
//Boost
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/graph/graphviz.hpp>
#include "boost/graph/topological_sort.hpp"
#include <boost/graph/graph_traits.hpp>
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/progress.hpp"
#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/prim_minimum_spanning_tree.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/graph_utility.hpp>
#include <boost/graph/tiernan_all_cycles.hpp>
#include <boost/graph/breadth_first_search.hpp>


//global variables (badman tings)
bool saveImg = true;
double big_constant = 10e32;
int thresh = 1; //actually number of standard deviations away from straight line point needs to be to be conserved
int resolution = 20; //every resolution images in the point vector will be checked for preservation

//define graph types, top one is used, point is the vertex data 
typedef boost::adjacency_list< boost::listS, boost::vecS, boost::undirectedS,  Point> Graph; 
typedef boost::adjacency_list< boost::vecS, boost::vecS, boost::undirectedS, Point> GraphVec;
//define boost iterator-type objects
typedef boost::graph_traits < Graph >::vertex_descriptor v_d;
typedef boost::graph_traits<Graph>::vertex_iterator v_i;
typedef std::pair<boost::graph_traits<Graph>::adjacency_iterator, boost::graph_traits<Graph>::adjacency_iterator> adjacency_range;

//typedef boost::property_map<Graph, Point>::type IndexMap;
//typedef boost::property_map<Graph, boost::vertex_color_t>::type colorMap;
//different connectivity types in terms of relative pixel positions to (0,0)
std::vector<Point> shifts4 = { Point(1,0), Point(0,1), Point(-1,0), Point(0,-1) }; //4 points sharing side with given point + shape 
std::vector<Point> shifts8 = { Point(1,0), Point(1,1), Point(0,1), Point(-1,1), Point(-1,0), Point(-1,-1), Point(0,-1), Point(1,-1) }; //8 points sharing vertex with centre point
std::vector<Point> shifts8cw={ Point(-1,-1),Point(0,-1),Point(1,-1),Point(1,0),Point(1,1),Point(0,1),Point(-1,1),Point(-1,0) };
std::vector<Point> shifts24 = { Point(-2,-2), Point(-1,-2), Point(0,-2), Point(1,-2), Point(2,-2), Point(-2,-1), Point(-2,0), Point(-2,1),Point(-2,2), //ring around shifts8
								Point(-1,2),Point(0,2),Point(1,2),Point(2,2),Point(2,1),Point(2,0),Point(2,-1) };

//we need this in order to use Point as a key in a map
//sort by x index, or y if x indices are equal
//we need maps to store point-vertex descriptor pairs
//as the vertex from property function in boost is 
//very fussy and goes wrong loads
struct Compare_Points
{
	bool operator() (cv::Point const& a, cv::Point const& b) const
	{
		if (a.x < b.x) return true;
		if (a.x == b.x && a.y < b.y) return true;
		return false;
	}
};
//for a given vertex in a given graph return vector of points
//in that vertex's 8-neighbourhood, if that point is inside the image
void neighbourCheck(v_d v, Graph& g, Size size, std::vector<Point>& shifts, std::vector<Point>& neighbours) { //this is a bit messy, hopefully it works at least
	Point p_a = g[v];
	neighbours.clear();
	//std::vector<Point> neighbouringPoints;
	for (Point p_it : shifts) {
		Point p = p_it + p_a;
		if ((p.x >= 0 && p.x<size.width) && (p.y >= 0 && p.y<size.height))
			neighbours.push_back(p);

	}
	//return neighbouringPoints;
}
void neighbourCheck(Point p_a, Size size, std::vector<Point>& shifts, std::vector<Point>& neighbours) {
	neighbours.clear();
	for (Point p_it : shifts) {
		//cout << "comparing " << difference << " with " << p << "\n";
		Point p = p_it + p_a;
		if ((p.x >= 0 && p.x<size.width) && (p.y >= 0 && p.y<size.height))
			neighbours.push_back(p);
	}
}

//check is pixel is at edge of image
bool Is_Boundary(Point p, Point size)
{
	if (p.x == 0 or p.y == 0 or p.x == size.x - 1 or p.y == size.y - 1) return true;
	return false;
}
//check if one of a given group of points touches boundary
bool Is_Boundary(std::vector<Point>const& points, Point size)
{
	for (auto p : points)
		if (Is_Boundary(p, size)) return true;
	return false;
}
//define boundary of image
bool Boundary(Mat_<bool>const& mask, std::vector<bool>& boundary)
{
	int row = 0, col = 0;
	
	for (row = 0, col = 0; row < mask.rows; row++) boundary.push_back(mask.ptr<bool>(row)[col]);
	for (row = mask.rows - 1, col = 0; col < mask.cols; col++) boundary.push_back(mask.ptr<bool>(row)[col]);
	for (row = mask.rows - 1, col = mask.cols - 1; row >= 0; row--) boundary.push_back(mask.ptr<bool>(row)[col]);
	for (row = 0, col = mask.cols - 1; col >= 0; col--) boundary.push_back(mask.ptr<bool>(row)[col]);
	return true;
}

bool Boundary_Vertices(Mat_<bool>const& mask, int& boundary_vertices)
{
	boundary_vertices = 0;
	std::vector<bool> boundary;
	Boundary(mask, boundary);
	int changes = 0;
	for (int i = 0; i < boundary.size(); i++)
		if (boundary[i] != boundary[(i + 1) % boundary.size()]) changes++;
	if (changes % 2 != 0)
	{
		std::cout << "\nError in Remove_Small_Components";
		return false;
	}
	boundary_vertices = int(changes / 2);
	std::cout << "Boundary_Verticies complete\n";
	return true;
}
bool Add_Edge(Mat_<bool>const&mask, bool object, Graph&graph, Point& pa, Point& pb, std::map< Point, v_d, Compare_Points >& pixels_vertices) {

	if (mask.ptr<bool>(pa.y)[pa.x] != object||mask.ptr<bool>(pb.y)[pb.x] != object) return false;
	if (!edge(pixels_vertices[pa], pixels_vertices[pb], graph).second) {
		boost::add_edge(pixels_vertices[pa], pixels_vertices[pb], graph);
		//std::cout << "edge added " << pa << " to " << pb << "\n";
		return true;
	}
}

bool Pixels_to_Graph_Edges(Mat_<bool>& mask, bool object, Graph& graph, bool fourconnect, std::map< Point, v_d, Compare_Points >& pixels_vertices) {
	for (int row = 0; row < mask.rows; row++) {
		//std::cout << row << "\n";
		for (int col = 0; col < mask.cols; col++)
		{
			Point p0, p1, p2, p3;
			p0 = Point(col, row);
			if (mask.ptr<bool>(p0.y)[p0.x] == object) {
				if (col != mask.cols - 1) {
					p1 = Point(col + 1, row);
					Add_Edge(mask, object, graph, p0, p1, pixels_vertices);
				}
				if (row != mask.rows - 1) {
					p2 = Point(col, row + 1);
					Add_Edge(mask, object, graph, p0, p2, pixels_vertices);
				}
				if ((!fourconnect) && (row != mask.rows - 1) && (col != mask.cols - 1)) {
					p3 = Point(col + 1, row + 1);
					Add_Edge(mask, object, graph, p0, p3, pixels_vertices);
				}
			}
		}
	}
	//std::vector<Point> shifts,surrounding;
	//if (fourconnect)shifts = shifts4;
	//else shifts = shifts8;
	//BGL_FORALL_VERTICES(v, graph, Graph) {		
	//	Point subject = graph[v];
	//	neighbourCheck(v, graph, mask.size(),shifts,surrounding);
	//	for (auto p : surrounding) {
	//		if (mask.ptr<bool>(p.y)[p.x]=object) { //if point exists
	//			auto v2 = pixels_vertices[p];
	//			if (!edge(v, v2, graph).second) add_edge(v, v2, graph);
	//		}
	//	}
	//	surrounding.clear();
	//}
	return true;
}

bool Pixels_to_Graph(Mat_<bool>& mask, bool object, Graph& graph, bool fourconnect) {//Original version
	graph.clear();
	std::cout << "graph cleared\n";
	std::map< Point, v_d, Compare_Points > pixels_vertices;
	// Add vertices
	std::cout << "adding verticies...\n";
	for (int row = 0; row < mask.rows; row++) {
		for (int col = 0; col < mask.cols; col++)
		{
			if ((mask.ptr<bool>(row)[col] != object)) continue; // irrelevant pixel
			auto vertex = boost::add_vertex(Point(col, row), graph);
			pixels_vertices.insert(std::make_pair(Point(col, row), vertex));
		}	
	}
	Pixels_to_Graph_Edges(mask, object, graph, fourconnect, pixels_vertices);
	return true;
}
//using external map and vertices already in map
bool Pixels_to_Graph(Mat_<bool>& mask, bool object, Graph& graph, bool fourconnect, std::map< Point, v_d, Compare_Points >& pixels_vertices) {

	Pixels_to_Graph_Edges(mask, object, graph, fourconnect, pixels_vertices);
	return true;
}

//magnitude of a point 
float magnitudeP(Point p) {
	return std::sqrt((p.x*p.x) + (p.y*p.y));
}

//perpendicular distance from point to a line defined by two other points
double distanceToLine(Point ptest,Point pline1,Point pline2 ) {
	int dy = pline2.y - pline1.y;
	int dx= pline2.x - pline1.x;
	double numerator = abs((ptest.x*dy) - (ptest.y*dx) + (pline2.x*pline1.y) - (pline2.y*pline1.x));
	double denominator = std::sqrt((dy*dy) + (dx * dx));
	return (numerator) / (denominator);
}

/*in the graph the degree 3 points we are interested in are often clustered,
* we want to treat these as a single point so a data structure for keeping 
* track of these will be useful
*/
class vertexCluster{
public:
	std::vector<v_d> constituents;
	Point centre;
	v_d centreVD;
	int ID,size;
	//constructor/destructor
	vertexCluster(){}
	~vertexCluster(){}

	void addVertex(v_d vertex) {
		constituents.push_back(vertex);
		
	}

	void addVertices(std::vector<v_d>& vertex) {
		constituents=vertex;
	}
	//calculate centre point of a vertex cluster
	Point getCentre(Graph& g) {
		if (constituents.size() == 1) {
			return g[constituents[0]];
		}
		size = constituents.size();
		int xsum=0,ysum=0,highestorder;
		for (v_d v : constituents) {			
			xsum += g[v].x;
			ysum += g[v].y;
		}
		centre = Point(round(xsum / constituents.size()), round(ysum / constituents.size()));
		//ensure centre point is one of the points (for convenience)
		bool centreInSet;
		for (v_d v : constituents) {
			if (g[v] == centre) {
				centreInSet = true;
				centreVD = v;
				break;
			}
			else { centreInSet = false; }
		}
		if (!centreInSet) {
			centre = g[constituents.front()];
			centreVD = constituents.front();
		}
		return centre;
		//std::cout << "centre calculated to be " << centre << "\n";
	}
};

//calculate standard deviation for a given vector of floats
//returns pair containing mean and stdev
std::pair<float,float> stdDev(std::vector<float> &input) {
	float sum=0, mean=0, intermediate=0,sigSq=0;
	std::vector<float> differencesSqd;
	sum = std::accumulate(input.begin(),input.end(),0);
	mean = sum / input.size();
	for (float f : input) {
		intermediate =f - mean;
		differencesSqd.push_back(intermediate*intermediate);
	}
	sigSq = std::accumulate(differencesSqd.begin(), differencesSqd.end(), 0)/input.size();
	differencesSqd.clear();

	return std::make_pair(mean,std::sqrtf(sigSq));
}

//when sorting edge cluster we need to check if there are points
//still to sort, this is done through point-boolean pairs 
//to check we iterate over whole map and return true as soon
//as we see one true value
bool mapIsTrue(std::map<Point,bool,Compare_Points>& map) {
	for (auto it = map.begin(); it != map.end();++it) {
		if(it->second)return true;
	}
	return false;
}

void sortCluster(std::vector<v_d>& input, std::vector<v_d>&output, std::pair<v_d, v_d> ends, Graph& g) {
	v_d start = ends.first;
	v_d end = ends.second;
	v_d current = start;
	//std::cout << "processing points from " << g[start] << " to " << g[end]<<"\n";
	std::map<Point, bool, Compare_Points> unchecked;
	for (v_d v : input) {
		unchecked.insert(std::make_pair(g[v], true));
	}

	while (mapIsTrue(unchecked)) {

			output.push_back(current);
			//for some reason if remove_small_Components (slowest function) is commented out 
			//there starts to be nonsense points around 2000 points in which crashes this,
			//hence catching the exception
			try {
				unchecked.at(g[current]) = false;
			}
			catch (...) {
				std::cout << "point " << g[current] << " does not exist in this map \n";
			}
			auto adj = adjacent_vertices(current, g);
			for (auto a = adj.first; a != adj.second; ++a) {
				if ((unchecked.find(g[*a]) != unchecked.end())) {//if point exists 
					if (unchecked.at(g[*a])) //if point is unchecked
						current = *a; //use this as new point
				}
			}
	}
	
}
//as well as keeping track of centre points we will keep track of the edge-like order 2 chains
//lots of redundant stuff in this class.
class edgeCluster {
public:
	//Point begin, end;
	std::pair<Point, Point> ends;
	std::pair<v_d,v_d> endsVD;
	std::vector<Point> conserved;
	v_d beginV, endV,conservedV;
	std::vector<v_d> constituents;
	std::vector<v_d> sorted;
	float dist, stdev;
	std::vector<float> distances;
	bool conservedPoints=false;
	edgeCluster(){}
	void addVertex(v_d vertex) {
		constituents.push_back(vertex);
	}
	std::vector<Point> getImportantPoints(Graph& g, int sigma) {
		v_d myPoints[]{-1,-1};
		int indexer = 0;
		float maxdistance = 0;
		if (constituents.size()==1) {
			conservedPoints = false;
			return { g[constituents[0]] };
		}
		else if (constituents.size() == 2) {
			conservedPoints = false;
			return{ g[constituents[0]],g[constituents[1]] };
		}
		else {
			//std::cout << constituents.size() << "constituent vertices\n";
			for (v_d v : constituents) {
				//std::cout << "checking " << v << "\n";
				if (degree(v, g) == 1) {

					myPoints[indexer] = v;
					//std::cout << v << " " << g[v] << "\n";
					++indexer;
				}
			}
		
			ends = std::make_pair(g[myPoints[0]], g[myPoints[1]]); //end points in terms of Points
			endsVD = std::make_pair(myPoints[0], myPoints[1]); //end points in terms of vertex descriptors
			sortCluster(constituents, sorted, endsVD, g); //sort cluster (not needed if we try different edge way)
			
			for (v_d v : sorted) { //probably inefficient to calculate all of these when we only use 10% of them...
				if (v != myPoints[0] && v != myPoints[1]) {
					dist = distanceToLine(ends.first, ends.second, g[v]);
					distances.push_back(dist);
					//std::cout << "distance: " << dist << "\t";
				}
			}
			//calculate threshold for point conservation
			auto stats = stdDev(distances);
			float threshold = stats.first + sigma * stats.second;

			int index = 0;

			conserved.push_back(g[sorted.front()]);
			//calculate points to be saved 
			for (auto v:sorted) {

				if (index > 0 && index < sorted.size()) {
					
					if (distances[index] > threshold  && (index%resolution==0)) {
						maxdistance = dist;
						conserved.push_back(g[v]);
						conservedV = v;
						conservedPoints = true;

					}
				}
				++index;
			}
			conserved.push_back(g[sorted.back()]);
			return conserved;
		}
	}


};
//define data structure for storing all data needed to reconstruct skeleton
typedef std::tuple<std::vector<Point>*, std::vector<edgeCluster>*, std::vector<std::vector<Point>>*> graphData;

//the simplified skeleton has some issues...
//try to work out what the hell is going on here
//by printing out bad-degree vertices and their neighbours
void diagnoseShitGraph(Graph&g) {
	int zeros = 0, ones = 0;
	BGL_FORALL_VERTICES(v, g, Graph) {
		int deg = degree(v, g);
		if (deg==0 || deg==1) {
			switch (deg) {
			case(0):
				++zeros;
			case(1):
				++ones;			
			}
			std::cout << "ACHTUNG! point " << g[v] << " has degree " << degree(v, g) << "its neighbours:\n";
			auto adj = adjacent_vertices(v, g);
			for (auto a = adj.first; a != adj.second; ++a) {
				std::cout << g[*a] <<" degree"<<degree(*a,g)<<"\t";
			}
			std::cout << "\n";
		}
	}
	std::cout << format("zeros: %i, ones: %i\n", zeros, ones);
}

//create image from a vector of points
void printPointVec(Size size, std::vector<Point>& input) {
	Mat out = Mat(size, CV_8UC3, Vec3b(232, 232, 232));
	for (Point p : input) {
		out.at<Vec3b>(p) = Vec3b(50, 50, 50);
	}
	imwrite("centrepoints.png", out);
}
//print out graph from reconstructed edges
void printEdges(Graph& g, Size size,String name,int regions, Mat& img_og, Mat& img_inter) {
	Mat img_original;
	Mat img_int;
	img_og.copyTo(img_original);
	img_inter.copyTo(img_int);
	Mat canvas = Mat(size, CV_8UC3, Vec3b(255, 255, 255));
	Vec3b colour;
	int radius;
	//draw edges
	BGL_FORALL_EDGES(e, g, Graph) {
		line(canvas, g[source(e, g)], g[target(e, g)], Vec3b(0, 0,0), 1);
		line(img_original, g[source(e, g)], g[target(e, g)], Vec3b(0, 0, 0), 1);
		line(img_int, g[source(e, g)], g[target(e, g)], Vec3b(0, 0, 0), 1);
	}

	//draw vertices
	BGL_FORALL_VERTICES(v, g, Graph) {
		int deg = degree(v, g);
		if (deg == 3) {
			colour = Vec3b(255,0, 0);
			radius = 2;
		}
		else if(deg==2){
			colour = Vec3b(0, 0, 0);
			radius = 1;
		}
		else if(deg==1) {
			colour = Vec3b(0, 0,255);
			radius = 1;
		}		
		else if (deg == 4) {
			colour = Vec3b(255, 0, 0);
			radius = 1;
		}
		else if (deg == 0) {
			colour = Vec3b(255, 0, 0);
			radius = 1;
		}
		if (deg != 2)
			//canvas.at<Vec3b>(g[v])= colour;
			circle(canvas,g[v],radius, colour, -1);
			circle(img_original, g[v], radius, colour, -1);
			circle(img_int, g[v], radius, colour, -1);
	}
	//put image name and number of regions on output image
	if (regions != 0) {
		String myText = name + " " + std::to_string(regions) + " regions";
		int baseline = 0;
		Size textSize = getTextSize(myText, FONT_HERSHEY_SIMPLEX,1, 2, &baseline);
		baseline += 2;
		Point textOrg((canvas.cols - textSize.width), (canvas.rows - textSize.height));
		putText(canvas, myText,textOrg, FONT_HERSHEY_SIMPLEX, 1, Scalar(200, 200, 200), 2);
		putText(img_original, myText, textOrg, FONT_HERSHEY_SIMPLEX, 1, Scalar(200, 200, 200), 2);
		putText(img_int, myText, textOrg, FONT_HERSHEY_SIMPLEX, 1, Scalar(200, 200, 200), 2);
	}

	if (saveImg) {
		cv::imwrite(name+"_skeleton.png", canvas);
		cv::imwrite(name + "_original.png", img_original);
		cv::imwrite(name + "_intermediate.png", img_int);
	}
}
//print out the graphs where every vertex represents an 
//individual pixel so we can just draw all vertices 
//and color based upon it's respective degree
void printGraph(Graph& g, Size size,String outname) {
	Point current;
	Mat canvas = Mat(size, CV_8UC3, Vec3b(51, 51, 51));
	int order;
	int loopCounter = 0;

	BGL_FORALL_VERTICES(v, g, Graph) {
		current = g[v];// Point(g[v].y, g[v].x);
					   //std::cout << "current pixel: " << current << "\n";
		order = degree(v, g);
		switch (order) {
		case 0:
			canvas.at<Vec3b>(current) = Vec3b(127, 127, 127); //grey
															
			break;
		case 1:
			canvas.at<Vec3b>(current) = Vec3b(0, 100, 200); //orange
															
			break;
		case 2:
			canvas.at<Vec3b>(current) = Vec3b(204, 204, 204); //white
															
			break;
		case 3:
			canvas.at<Vec3b>(current) = Vec3b(20, 204, 10); //green
														
			break;
		case 4:
			canvas.at<Vec3b>(current) = Vec3b(200, 10, 255); //purple														
			break;
		case 5:
			canvas.at<Vec3b>(current) = Vec3b(0, 10, 255); //red													
			break;
		}
	}

	if(saveImg) imwrite(outname+".png", canvas);
	
}





//find vertices which form the edges of the graph
//this is the naive approach and points still need sorting
//a method which 'walks' through the graph may be more efficient
//and could find edges and vertex clusters in a single traversal
//of the graph 
void findClusterEdges(Graph& g, Size size, std::vector<std::vector<Point>>*endPoints, std::vector<edgeCluster>*ec) { //lots of duplicate code with findclusters, these could be combined for smaller binaries, cba atm tho
	std::cout << "finding edge clusters...\n";
	Graph g_edges;
	Mat_<bool>myEdges(size, false);
	v_d init = -1;
	std::vector<int> intToVD;
	Mat_<v_d> descriptors =Mat_<v_d>::zeros(size);
	
	//add vertex for every degree 2 vertex in skeleton graph
	BGL_FORALL_VERTICES(v, g, Graph) {
		if (degree(v, g) == 2) {
			auto s = add_vertex(g_edges);
			intToVD.push_back(s);
			g_edges[s] = g[v];
			//std::cout << g[v] << "\n";
			descriptors.at<v_d>(g_edges[s]) = s;
			myEdges.at<bool>(g[v]) = true;
		}
	}

	std::vector<Point> neighbours;
	for (int j = 0; j < myEdges.rows; ++j) {
		for (int i = 0; i < myEdges.cols; ++i) {
			if (myEdges.at<bool>(Point(i, j))) {
				neighbourCheck(Point(i, j), myEdges.size(),shifts8, neighbours);
				for (Point n : neighbours) {
					if (myEdges.at<bool>(n)) {
						v_d v1 = descriptors.at<v_d>(Point(i, j));
						v_d v2 = descriptors.at<v_d>(n);
						if(!edge(v1,v2,g_edges).second)
							add_edge(v1, v2, g_edges);
					}
				}
			}
		}
	}
	//split edges into seperate objects by using connected components
	std::vector<int> component(num_vertices(g_edges));
	int num = connected_components(g_edges, &component[0]);
	std::cout << "Total number of edge clusters: " << num << " number of vertices " << num_vertices(g_edges) << std::endl;
	//create edge cluster object for each component
	for (int i = 0; i < num; ++i) {
		ec->push_back(edgeCluster());
	}

	for (int i = 0; i < component.size(); ++i) {

		if (ec->operator[](component[i]).constituents.empty()) {
			ec->operator[](component[i]).addVertex(intToVD[i]);
		}
		else {
			ec->operator[](component[i]).constituents.push_back(intToVD[i]);
		}
	}
	int count = 0;
	for (auto ecit : *ec) {		
		//calculate end points, sort vertices, and decide which vertices to save, for every edgeCluster object
		endPoints->push_back(ecit.getImportantPoints(g_edges,thresh));

		++count;
	}
	//display edges
	printGraph(g_edges, size,"cluster_edges");	
}

//get all degree>2 points, connect them accordingly, do connected components
//(basically the same as edge clusters, just we calculate a centre point instead)
void findClusters(Graph& g, Size size, std::vector<Point>* branchPoints) { 
	Graph clusters;
	
	std::cout << "identifying branch vertices\n";
	
	Mat_<bool> clustermask(size, false);
	std::map<Point, Graph::vertex_descriptor, Compare_Points> pointVerts;
	auto verts = vertices(g);
	//
	for (auto v = verts.first; v != verts.second; ++v) {
		if (degree(*v, g) > 2) {
			auto newv = add_vertex(g[*v], clusters);
			pointVerts.insert(std::make_pair(g[*v], newv));
			clustermask.ptr<bool>(g[*v].y)[g[*v].x] = true;
		}
	}

	Pixels_to_Graph(clustermask, true, clusters, false, pointVerts);
	std::vector<int> component(num_vertices(clusters));
	
	int num = connected_components(clusters, &component[0]);
	std::cout << "Total number of vertex clusters: " << num <<" comprising of "<< num_vertices(clusters)<<std::endl;
	
	
	std::vector<vertexCluster> vc;// = new std::vector<vertexCluster>;
	for (int i = 0; i < num; ++i) {
		vc.push_back(vertexCluster());
	}

	for (int i = 0; i < component.size();++i) {
		//std::cout << "vertex " << clusters[i] << " " << " is in component " << component[i] << "\n";
		if (!vc[component[i]].constituents.data()) {
			vc[component[i]].addVertex(pointVerts[clusters[i]]);
		}
		else {
			vc[component[i]].constituents.push_back(pointVerts[clusters[i]]);
		}
	}
	int count = 0;
	//std::cout << vc.size() << "\n";
	for (auto vcit : vc) {
		Point centre=vcit.getCentre(clusters);
		//std::cout << centre << "\n";
		branchPoints->push_back(centre);
	}
	printGraph(clusters, size, "cluster_vertices");
	//vc.clear();
}

//function to call the other functions to make the image_to_Graph function less messy
//relevant results stored in tuple of pointers to vectors of points/edge clusters
graphData classifyGraph(Graph& graph, Size size,std::vector<Point>* verts,std::vector<edgeCluster>* edges,
						std::vector<std::vector<Point>>*edgeEnds)
{
	findClusters(graph, size, verts);
	findClusterEdges(graph, size, edgeEnds,edges);
	printPointVec(size, *verts);
	return std::make_tuple(verts, edges,edgeEnds);
}

//find 2 closest vertices to a given point
//(doesnt need to be a pair just the closest is fine)
std::pair<v_d,v_d> closestVertex(Graph& g, Point p) {
	v_d currentclosest=-1,previousclosest=-1;
	std::vector<v_d>closests;
	closests.clear();
	Point pointVec;
	float distance;
	float difference=1000;
	BGL_FORALL_VERTICES(v, g, Graph) {
		pointVec = p - g[v];
		previousclosest == currentclosest;
		float distance=norm(pointVec);
		if (distance < difference) {
			difference =distance;
			closests.push_back(v);
			currentclosest = v;
		}
	}
	
	return std::make_pair(closests[closests.size() - 1], closests[closests.size() - 2]);
}

v_d findClosestVectorElement(Point checkthis, std::vector<Point>* branchPoints, std::map<Point, v_d, Compare_Points>*pointVerts) {
	std::vector<float> results(branchPoints->size()); //store results in vector
	for (int i = 0; i < branchPoints->size();++i) { //looping over indices
		results[i] = norm(checkthis - branchPoints->operator[](i)); //fill results with distance between focus point and all other branches
	}
	auto a = std::min_element(results.begin(), results.end()); //get iterator to smallest value in results vector
	int index = a - results.begin(); //difference between smallest element ptr and first element ptr gives index in vector
	Point closest = branchPoints->operator[](index); //extract point value for required index 
	return pointVerts->operator[](closest);//return associated mapped vertex descriptor value for the point key
}
/*
* this function removes all order 2 vertices giving a new graph
* consisting only of non-trivial branch vertices, good for quick
* computer analysis of the graph but some detail in the visual
* R2 embedding is lost, less good for human analysis 
*/
void simplestGraph(Size size,String name, graphData data,Mat& img_orig, Mat& img_int ) {

	Graph simplest;
	std::vector<Point>* branchVerts = std::get<std::vector<Point>*>(data);
	std::map<Point, v_d, Compare_Points>pointVerts;
	
	for (Point p :*branchVerts) {
		auto nu = add_vertex(p,simplest);
		pointVerts.insert(std::make_pair(p, nu));
	}
	for (auto e : *std::get<std::vector<std::vector<Point>>*>(data)) {
		auto cvs1 = findClosestVectorElement(e.front(),branchVerts,&pointVerts);
		auto cvs2 = findClosestVectorElement(e.back(), branchVerts, &pointVerts);

		v_d cv1 = cvs1;
		v_d cv2 = cvs2;

		if (simplest[cv1]!=simplest[cv2]) {
			add_edge(cv1, cv2, simplest);
		}
	}
	BGL_FORALL_VERTICES(v, simplest, Graph) { //try to fix most the order 1 vertices
		int deg = degree(v, simplest);
		if (deg <2) {
			for (Point p : shifts8) {
				Point check = simplest[v] + p;
				auto vertcheck = pointVerts.find(check);
				if (vertcheck != pointVerts.end()) { //if point exists in graph
					if (!edge(v, vertcheck->second, simplest).second && degree(v, simplest) < 4) {
						add_edge(v, vertcheck->second, simplest);
					}
				}
			}
		}
	}
	int regions = (num_edges(simplest) + 2) - num_vertices(simplest);
	//diagnoseShitGraph(simplest);
	printEdges(simplest, size, name + "_simplest",regions,img_orig,img_int);
	std::cout << regions << " regions in " << name << "\n";
}
/*
*this function works in much the same way simplestGraph(),
* except some order-2 vertices are preserved in order to
* better retain the original R2 structure
*/
void simplerGraph(Size size, String name, graphData data, Mat& img_orig, Mat& img_int) {
	Graph simpler;
	Point prev;
	v_d nuprev;
	//map vertex descriptors to points
	std::map<Point, v_d, Compare_Points>pointVerts;
	//extract branching points 
	std::vector<Point>* branchVerts = std::get<std::vector<Point>*>(data);

	//add vertex to new graph for branch points
	for (Point p : *branchVerts) {
		auto nu = add_vertex(p, simpler);
		pointVerts.insert(std::make_pair(p, nu));

	}
	//extract all edge clusters 
	for (auto e : *std::get<std::vector<std::vector<Point>>*>(data)) {
		bool conserved = e.size() > 2;
		auto cvs1 = findClosestVectorElement(e.front(), branchVerts, &pointVerts);
		auto cvs2 = findClosestVectorElement(e.back(), branchVerts, &pointVerts);

		v_d cv1 = cvs1;
		v_d cv2 = cvs2;
		if (conserved) { //only include this extra point if it's needed
			for (auto p : e) {
				if (p == e.front()) {
					//special case start point-no edges added
					//add_edge(closestVertex(simpler, e.front()), nu, simpler);
					prev = p;
					nuprev = cv1;
					continue;
				}
				if (p == e.back()) {
					//special case back point, edge added to closest branch
					//auto cv = closestVertex(simpler, e.back()).first;
					if (!edge(nuprev, cv2, simpler).second) {
						add_edge(nuprev, cv2, simpler);
					}
				}
				else {
					//connect intermediate points in edge
					auto nu = add_vertex(simpler);
					simpler[nu] = p;
					if (!edge(nuprev, nu, simpler).second) {
						add_edge(nuprev, nu, simpler);
					}
					nuprev = nu;
				}
			}
		}
		if (!conserved && !edge(cv1, cv2, simpler).second && cv1 != cv2) { //just join end points if no points conserved
			add_edge(cv1, cv2, simpler);
		}
	}
	BGL_FORALL_VERTICES(v, simpler, Graph) { //try to fix most the order 1 vertices
		int deg = degree(v, simpler);
		if (deg < 2) {
			for (Point p : shifts8) {
				Point check = simpler[v] + p;
				auto vertcheck = pointVerts.find(check);
				if (vertcheck != pointVerts.end()) { //if point exists in graph
					if (!edge(v, vertcheck->second, simpler).second && degree(v, simpler) < 4) {
						add_edge(v, vertcheck->second, simpler);
					}
				}
			}
		}

		
	}
	diagnoseShitGraph(simpler);
	printEdges(simpler, size, name + "_simpler", (num_edges(simpler) + 2) - num_vertices(simpler), img_orig, img_int);
}

bool Save_Mask(Mat const& image, Mat const& mask, std::string name, Mat_<Vec3b>& image_mask) //this doing too much stuff currently should probably be 2-3 functions
{
	image_mask= Mat_<Vec3b>(image.size(), Vec3b(255,255,255));
	image.copyTo(image_mask, mask); //copy image to image_mask where mask is true (use ~mask for inverse)
	cv::imwrite(name, image_mask);
	return true;
}

//find true neighbours in mask for a given point
bool Neighbors(Mat_<bool>const& mask, Point point, std::vector<Point>const& shifts, int& num_neighbors, std::vector<bool>& neighbors)
{
	Point p;
	num_neighbors = 0;
	neighbors.assign(shifts.size(), true); //reset neighbours vector with shifts.size (i.e 4/8) true values 
	for (int i = 0; i < shifts.size(); i++)
	{
		p = point + shifts[i];
		if (p.x >= 0 and p.x < mask.cols and p.y >= 0 and p.y < mask.rows) //don't do stuff with pixels outside the image 
			neighbors[i] = mask.ptr<bool>(p.y)[p.x]; //access mask file
		if (neighbors[i]) num_neighbors++;//increment neighbours if value is true
	}
	return true;
}

//if a true point in the mask has no neighbours remove it 
//if point is false or has true neighbours do nothing
bool Remove_Isolated_Pixel(Point point, Mat_<bool>& mask)
{
	if (!mask.ptr<bool>(point.y)[point.x]) return false; // already removed
	int num_neighbors = 0;
	std::vector<bool> neighbors(shifts8.size(), true);
	Neighbors(mask, point, shifts8, num_neighbors, neighbors);
	if (num_neighbors == 0) { mask.ptr<bool>(point.y)[point.x] = false; return true; }
	return false;
}

bool Remove_External_Pixel(Point point, Mat_<bool>& mask)//CPU INTENSIVE!
{	//basically everything is memory access and comparisons, most expensive thing is the % on if statement and recursion
	// exceptional cases
	int px = point.x;
	int py = point.y;
	int mc = mask.cols;
	int mr = mask.rows;

	//if (point.x < 0 or point.y < 0 or point.x >= mask.cols or point.y >= mask.rows) return false;
	if (px < 0 or py < 0 or px >= mc or py >= mr) return false;
	if (Remove_Isolated_Pixel(point, mask)) return true;
	// corners aren't removed
	if (point == Point(0, 0)) return false;
	if (point == Point(0, mr - 1)) return false; // if (point == Point(0, mask.rows - 1)) return false;
	if (point == Point(mc - 1, 0)) return false; //if (point == Point(mask.cols - 1, 0)) return false;
	if (point == Point(mc - 1, mr - 1)) return false; //if (point == Point(mask.cols - 1, mask.rows - 1)) return false;
													  //bool debug = false; if ( point.x == mask.cols-1 ) debug = true;
	int num_neighbors;
	std::vector<bool> neighbors(shifts8.size(), true);
	Neighbors(mask, point, shifts8, num_neighbors, neighbors);//update neighbours vector
	int changes = 0;
	for (int i = 0; i < shifts8.size(); i++)
		if (neighbors[i] != neighbors[(i + 1) % neighbors.size()]) changes++; //if consecutive pixels aren't equal, increment changes
																			  //if ( debug ) std::cout<<"\n"<<point<<" n="<<num_neighbors<<" c="<<changes;
	if (changes == 2 and num_neighbors <= 4)
	{
		mask.ptr<bool>(point.y)[point.x] = false; //if 2 consecutive pixels aren't equal, and 4 or fewer neighbours, change mask to false at this point 
		for (int i = 0; i < shifts8.size(); i++)
			if (neighbors[i]) //if neighbour of point is true, remove external pixels 
				Remove_External_Pixel(point + shifts8[i], mask); //repeat process for neighbours
	}
	
	return true;
}

bool Remove_External_Pixels(Mat_<bool>& mask)
{
	std::cout << "Remove_External_Pixels() running... " << mask.rows << " rows\n";
	for (int row = 0; row < mask.rows; row++) {
		//if (!row % 10) std::cout <<"Rows Processed: "<< row;
		for (int col = 0; col < mask.cols; col++) {

			if (mask.ptr<bool>(row)[col]) {
				Remove_External_Pixel(Point(col, row), mask);

			}
		//	if (!row % 255)std::cout << "\r"; //(char)8;
		}
	}
	std::cout << "remove external pixels completed\n";
	return true;
}

bool Add_Internal_Pixel(Point point, Mat_<bool>& mask)
{
	// exceptional cases
	if (point.x < 0 or point.y < 0 or point.x >= mask.cols or point.y >= mask.rows) return false; //deal with edge cases 
	int num_neighbors;
	std::vector<bool> neighbors(shifts4.size(), true);
	Neighbors(mask, point, shifts4, num_neighbors, neighbors);
	if (num_neighbors >= 3) // a pixel with at least 3 of 4 potential nearest neighbors is called external
	{
		mask.at<bool>(point) = true; //make point true if it is external 
		neighbors.assign(shifts8.size(), true); //reset neighbours vector all true
		Neighbors(mask, point, shifts8, num_neighbors, neighbors); //update neighbours vector
		for (int i = 0; i < shifts8.size(); i++) //same as last bit of remove external pixels
			if (neighbors[i]) Remove_External_Pixel(point + shifts8[i], mask);
			else Add_Internal_Pixel(point + shifts8[i], mask);
	}
	return true;
}

bool Add_Internal_Pixels(Mat_<bool>& mask)
{
	std::cout << "adding internal pixels " << mask.rows << " total rows\n";
	for (int row = 0; row < mask.rows; row++) {
		//if (!row % 10)std::cout << row;
		for (int col = 0; col < mask.cols; col++) {
			if (!mask.ptr<bool>(col) [row]) {
				Add_Internal_Pixel(Point(col, row), mask);
			}

		}
		//if (!row % 10)std::cout << "\r";
	}
	return true;
}



bool Remove_Small_Components(bool object, int area_min, Mat_<bool>& mask, int& num_components, Graph& graph) {//MAKE THIS FAAAST
	std::cout << "remove small components called\n";
	
	Pixels_to_Graph(mask, object, graph, true);
	std::cout << "pixels to graph done\n";
	std::vector<int> vertex_components(boost::num_vertices(graph));
	
	// Count the sizes of components
	num_components = boost::connected_components(graph, &vertex_components[0]);
	std::vector< int > component_sizes(num_components, 0);
	for (int i = 0; i != vertex_components.size(); ++i)
		component_sizes[vertex_components[i]]++; //histogram

	
	//std::cout<<"\nComponents:"; for ( int i = 0; i < component_sizes.size(); i++ ) std::cout<<" c"<<i<<"="<<component_sizes[i];
	// Select small components to remove
	std::map< int, std::vector<Point> > small_components;
	std::vector<Point> empty;
	std::cout << "area min= " << area_min << "\n";
	if (object) area_min = *max_element(component_sizes.begin(), component_sizes.end()); // keep only the largest foreground object
	for (int i = 0; i < component_sizes.size(); i++)
		if (component_sizes[i] < area_min) small_components.insert(std::make_pair(i, empty));
	// Mark pixels from small components
	for (int i = 0; i != vertex_components.size(); ++i)
	{
		auto it = small_components.find(vertex_components[i]);
		if (it != small_components.end()) (it->second).push_back(graph[i]);
	}
	num_components -= (int)small_components.size();
	// Check if any small components touches the boundary
	if (object) for (auto it = small_components.begin(); it != small_components.end(); it++)
		if (Is_Boundary(it->second, Point(mask.cols, mask.rows))) // keep components touching the boundary
		{
			(it->second).clear();
			num_components++;
		}//\n
		 // Remove superfluos pixels
	std::cout << "removing superfluous pixels\n";
	int debugLoopCount = 0;
	for (auto it = small_components.begin(); it != small_components.end(); it++) {
		//std::cout << debugLoopCount;
		for (auto p : it->second) {
			mask.ptr<bool>(p.y)[p.x] = !object;
		}
		//debugLoopCount++;
		//std::cout << "\r";
	}


	return true;
}

bool getPotentialEnds(std::vector<v_d>& potentialEnds, Graph& g) {

	BGL_FORALL_VERTICES(v, g, Graph) {
		if (degree(v, g) == 1)potentialEnds.push_back(v);
	}
	return true;
}
/**
* Code for thinning a binary image using Guo-Hall algorithm.
*
* Perform one thinning iteration.
* Normally you wouldn't call this function directly from your code.
*
* @param  im    Binary image with range = 0-1
* @param  iter  0=even, 1=odd
*/
void thinningGuoHallIteration(cv::Mat& im, int iter)
{
	cv::Mat marker = cv::Mat::zeros(im.size(), CV_8UC1);

	for (int i = 1; i+1 < im.rows; i++)
	{
		for (int j = 1; j+1 < im.cols; j++)
		{
			uchar p2 = im.ptr<uchar>(i - 1)[ j];
			uchar p3 = im.ptr<uchar>(i - 1)[ j + 1];
			uchar p4 = im.ptr<uchar>(i)[j + 1];
			uchar p5 = im.ptr<uchar>(i + 1)[j + 1];
			uchar p6 = im.ptr<uchar>(i + 1)[j];
			uchar p7 = im.ptr<uchar>(i + 1)[j - 1];
			uchar p8 = im.ptr<uchar>(i)[j - 1];
			uchar p9 = im.ptr<uchar>(i - 1)[j - 1];

			int C = (!p2 & (p3 | p4)) + (!p4 & (p5 | p6)) +
				(!p6 & (p7 | p8)) + (!p8 & (p9 | p2));
			int N1 = (p9 | p2) + (p3 | p4) + (p5 | p6) + (p7 | p8);
			int N2 = (p2 | p3) + (p4 | p5) + (p6 | p7) + (p8 | p9);
			int N = N1 < N2 ? N1 : N2;
			int m = iter == 0 ? ((p6 | p7 | !p9) & p8) : ((p2 | p3 | !p5) & p4);

			if (C == 1 && (N >= 2 && N <= 3) & m == 0)
				marker.ptr<uchar>(i) [j] = 1;
		}
	}

	im &= ~marker;
}

/**
* Function for thinning the given binary image
*
* @param  im  Binary image with range = 0-255
*/
void thinningGuoHall(cv::Mat& im)
{
	//im /= 255;

	cv::Mat prev = cv::Mat::zeros(im.size(), CV_8UC1);
	cv::Mat diff;

	do {
		thinningGuoHallIteration(im, 0);
		thinningGuoHallIteration(im, 1);
		cv::absdiff(im, prev, diff);
		im.copyTo(prev);
	} while (cv::countNonZero(diff) > 0);

	im *= 255;
}

bool boundaryGraphEdges(Graph& g, Mat_<bool>& skel, std::map< Point, Graph::vertex_descriptor, Compare_Points >& pixels_vertices) {
	Point subject;
	std::vector<Point> surrounding;
	//std::cout << "in boundarygraphedges " << num_vertices(g) << "\n";
	BGL_FORALL_VERTICES(v, g, Graph) {
		surrounding.clear();
		subject = g[v];
		//std::cout << "checking neigbours of " << g[v] << "\n";
		neighbourCheck(v, g, skel.size(),shifts8,surrounding);
		for (auto p : surrounding) {
			//std::cout << "checking " << p << "\n";
			if (skel.ptr<bool>(p.y)[p.x]) { //if point exists
				auto v2 = pixels_vertices[p];
				if (!edge(v, v2, g).second) {
					add_edge(v, v2, g);
					//std::cout << "edge added\n";
				}
			}
		}
		//surrounding.clear();
	}
	return true;
}
//this basically does the same thing as Pixels_to_graph 
//but that doesn't work when scanning the skeleton in for some reason
bool boundaryGraph(Graph& g, Mat_<bool>& skel) {
	//std::cout << "adding vertices\n";
	g.clear();
	std::map< Point, Graph::vertex_descriptor, Compare_Points > pixels_vertices;

	for (int i = 0; i < skel.rows; ++i)
	{
		//std::cout << "row " << i << "\n";
		auto p = skel.ptr<bool>(i);
		for (int j = 0; j < skel.cols; ++j)
		{
			//std::cout << p[j] << "\n";
			if (p[j] || Is_Boundary(Point(j, i), skel.size())) { //
				auto v = add_vertex(Point(j, i),g);
				//g[v] = Point(j, i);
				pixels_vertices.insert(std::make_pair(Point(j, i), v));
				//std::cout << "vertex added: "<<Point(j,i)<<" \n";				
			}
		}
	}
	//std::cout << num_vertices(g) << "\n";
	boundaryGraphEdges(g, skel, pixels_vertices);
	//Pixels_to_Graph_Edges(skel, true, g, false, pixels_vertices);
	return true;
}
class imgProcessor {
public:
	int size_small{50};
	double min_area{ 10 };
	std::vector<int> min_areas;
	Point image_sizes;
	String inputfolder, namebase, ext, outputfolder, name;
	std::multimap<int, Point>pixel_values;
	Mat_<bool> mask;
	Mat image;
	Mat_<Vec3b> image_mask;
	Mat_<int> states;
	std::vector<v_d> potentialEnds;
	Graph graph;

	imgProcessor(String _inputF, String _name_base, String _ext, String _outputF)
	{
		inputfolder = _inputF;
		namebase = _name_base;
		ext = _ext;
		outputfolder = _outputF;
		//for (int i = 1; i < 3; i++)min_areas.push_back(100 * i);
		min_areas={75};
	}

	bool image_to_graph(int index)
	{
		bool debug = true, save_images = true;
		if (debug) save_images = true;
		name = namebase + std::to_string(index)+"." + ext;// +std::to_string(index);
		std::cout << name << "\n";
		image = cv::imread(inputfolder + name  , CV_LOAD_IMAGE_COLOR);
		if (!image.data) { std::cout << "filepath "+ inputfolder + name+ " does not exist apparently\n"; return false; }
		image_sizes = Point(image.cols, image.rows);
		std::cout << image_sizes << "\n";
		String outpath = outputfolder + name;
		Mat image_gray;
		cvtColor(image, image_gray, CV_BGR2GRAY);

		
		//setPixelValues(image_gray);
		//mitigate some of the noise in the thresholded image
		//all of these iterations probably aren't needed
		adaptiveThreshold(image_gray, image_gray, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 351, -5);
		medianBlur(image_gray, image_gray, 3);
		dilate(image_gray, image_gray, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));		
		medianBlur(image_gray, image_gray, 3);		
		erode(image_gray, image_gray, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));
		dilate(image_gray, image_gray, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));
		medianBlur(image_gray, image_gray, 3);		
		erode (image_gray, image_gray, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));

		//extract contours of thresholded image
		//(not currently used but there's probably potential use in it's ability to extract
		//layers of features, i.e different graphs could be produced for 'island' features 
		//additionally if a rougher approximation than CHAIN_APPROX_TC89_KCOS exists/can be 
		//programmed this would eliminate the need for remove_small_components 
		//and speed up the program by a factor of 4
		std::vector<std::vector<Point>> contours;
		std::vector<Vec4i> hierachy;
		findContours(image_gray, contours, hierachy, RETR_TREE , CHAIN_APPROX_TC89_KCOS);
		Mat contoursImg = Mat(image_sizes, CV_8UC3,Scalar(50,50,50));
		//plot contours as solid (fill between inner and outer)
		drawContours(contoursImg, contours, -1, Scalar(200, 200, 200), FILLED, 4, hierachy);
		namedWindow("contours", WINDOW_NORMAL);
		imshow("contours",contoursImg );

		//generate mask
		mask = Mat_<bool>(image_sizes.y, image_sizes.x, false); //(y,x) important!!!
		
		for (int i = 0; i < mask.rows; ++i)
		{
			auto p = image_gray.ptr<uchar>(i);
			auto b = mask.ptr<bool>(i);
			for (int j = 0; j < mask.cols; ++j)
			{
				if (p[j] || i==0 || j==0 || (i==mask.rows-1) || (j==mask.cols-1)) b[j] = true;
				
			}
		}

		Save_Mask(image, mask, "_split" + std::to_string(size_small) + ".png", image_mask);
		//imwrite(name + "_split.png", image, mask);

		// Remove small objects
		int mas = min_areas.size();
		std::vector<int> num_vertices(mas), num_domains(mas), num_walls(mas);
		std::cout << "removing small objects for " << mas << " areas\n";
		Mat maskout;
		image.copyTo(maskout, mask);
		namedWindow("mask", WINDOW_NORMAL);
		imshow("mask",maskout );
		for (int i = 0; i < mas; i++)
		{
			/*THIS IS THE BIT THAT TAKES AAAAGES*/
			//straighten edges of mask and remove small/noise regions
			std::cout << "areas processed: " << i<<"\n";
			
			Remove_Small_Components(true, min_areas[i], mask, num_walls[i], graph); // true means foreground
			
		
			Remove_Small_Components(false, min_areas[i], mask, num_domains[i], graph); // false means background
			Remove_External_Pixels(mask); // causes the lines to go off centre, fast compared to remove small components though!
			
			Mat intermediate_out= Mat(image.size(), CV_8UC3, Scalar(255, 255, 255));
			image.copyTo(intermediate_out, mask);
			//Save_Mask(image, mask, outpath + "_no_holes" + std::to_string(min_areas[i]) + ".png",image_mask);

			Mat_<bool>skel(mask.rows, mask.cols,false);//
			Mat coloredSkel(skel.size(),CV_8UC3, Scalar(0,0,0));
			std::cout << "generating skeleton\n";
			
			mask.copyTo(skel);
			//_cvConstructSkeleton(skel);
			

			thinningGuoHall(skel);
			image.copyTo(coloredSkel, skel);
			namedWindow("skel", WINDOW_NORMAL);
			imshow("skel",coloredSkel);
			//Mat_<bool> fatskel;
			//dilate(skel, fatskel , getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));
			//Mat_<bool> skelInv = ~fatskel; //invert mask


			//copy original image to coloredSkel where inverse of fatskel is true
			//image.copyTo(coloredSkel, ~fatskel);

			//imwrite(name + "_skeletonised" + std::to_string(min_areas[i]) + ".png", coloredSkel);
			//imwrite(name + "_skeletonised" + std::to_string(min_areas[i]) + ".png", skel);
			graph.clear();
			//thinning(skel);
			//Graph skelgraph, seconditeration;


			

			boundaryGraph(graph, skel);
			//Pixels_to_Graph(skel, true, graph, false);

			printGraph(graph, skel.size(),"skeleton graph");
			
			//create pointers to data structures
			std::vector<Point>* verts = new std::vector<Point>;
			std::vector<std::vector<Point>>* edgeEnds = new std::vector<std::vector<Point>>;
			std::vector<edgeCluster>*edgeClusters = new std::vector<edgeCluster>;
			//call to function which calls the graphing functions
			graphData myData = classifyGraph(graph, skel.size(), verts, edgeClusters, edgeEnds);			
			simplestGraph(skel.size(), namebase + std::to_string(index), myData,image,intermediate_out);
			simplerGraph(skel.size(), namebase+ std::to_string(index), myData,image, intermediate_out);
			//delete pointers once used to stop memory overflow
			delete verts;
			delete edgeEnds;
			delete edgeClusters;
			

			
		}
		return true;
	}
};

void makeTitle() {
	//can't have a proper console application w/out ascii art...
	//use (char)0xB0 for ░ block
	//use (char)0xB1 for ▒ block
	//use (char)0xB2 for ▓ block
	//use (char)0xB9 for ╣ frame
	//use (char)0xBA for ║ frame
	//use (char)0xBB for ╗ frame
	//use (char)0xBC for ╝ frame
	//use (char)0xC8 for ╚ frame
	//use (char)0xC9 for ╔ frame
	//use (char)0xCC for ╠ frame
	//use (char)0xCD for ═ frame
	//replace \ with '\\'
	Mat logoimg = imread("logotext.bmp", IMREAD_GRAYSCALE);
	Mat_<char> logoascii = Mat_<char>(logoimg.size(), 0);
	for (int j = 0; j < logoimg.rows; ++j) {

		for (int i = 0; i < logoimg.cols; ++i) {
			Point p = Point(i, j);
			if (logoimg.at<uchar>(p) == 0)logoascii.at<char>(p) = 0xB2;
			else if (logoimg.at<uchar>(p) == 255)logoascii.at<char>(p) = 0xB0;
			else logoascii.at<char>(p) = 0xB1;
		}
	}
	int x = 0;
	std::cout << (char)0xC9;
	while (x < logoimg.cols) {
		std::cout << (char)0xCD;
		++x;
	}
	std::cout << (char)0xBB<<"\n";
	for (int j = 0; j < logoimg.rows; ++j) {
		std::cout << (char)0xBA;
		for (int i = 0; i < logoimg.cols; ++i) {
			std::cout << logoascii.at<char>(Point(i, j));
		}
		std::cout << (char)0xBA << "\n";
		
	}
	x = 0;
	std::cout << (char)0xCC;
	while (x < logoimg.cols) {
		std::cout << (char)0xCD;
		++x;
	}
	std::cout << (char)0xB9 << "\n";
	String text1 = "v0.1 - Eris Tricker - 2018";
	String text2 = "Based on code by Dr. Vitaliy Kurlin";
	int line1spacing =logoimg.cols - text1.size();
	int line2spacing = logoimg.cols - text2.size();
	std::cout << (char)0xCC;
	x = 0;
	while (x <= line1spacing) {
		if (x == line1spacing / 2)std::cout << text1;
		else std::cout << (char)0xCD;
		++x;
	}
	std::cout << (char)0xB9 << "\n" << (char)0xCC;
	x = 0;
	while (x <= line2spacing) {
		if (x == line2spacing / 2)std::cout << text2;
		else std::cout << (char)0xCD;
		++x;
	}
	std::cout << (char)0xB9 << "\n"<<(char)0xC8;
	x = 0;
	while (x < logoimg.cols) {		
		std::cout << (char)0xCD;
		++x;
	}
	std::cout << (char)0xBC << "\n";
}

int main(int argc, char* argv [])
{
	float t = (float)getTickCount();
	float tint=t;
	makeTitle();

	String input_folder = "C:\\Users\\tricker\\Pictures\\INPUT\\set\\";
	String output_folder = "C:\\Users\\tricker\\Pictures\\OUTPUT\\";
	String ext = "tiff";//jpeg, use .tiff for set;
	String name_base = "FC 100x";
	//Method method(input_folder, name_base, ext, output_folder);
	imgProcessor improc(input_folder, name_base, ext, output_folder);
	//std::cout<<"area_min="<<method.area_min;
	std::cout << "image (min_area, domains, vertices) ...\n";
	for (int i = 2; i <3; ++i) {
		float tint = (float)getTickCount();
		improc.image_to_graph(i);
		tint = ((double)getTickCount() - tint) / getTickFrequency();
		std::cout << "image "<<name_base<<i<<" processed, took "<<tint<<"s \n";
	}
	t = ((double)getTickCount() - t) / getTickFrequency();
	std::cout << "DONE! took " << t << " seconds\n";

	waitKey(0);
	return 0;
}


