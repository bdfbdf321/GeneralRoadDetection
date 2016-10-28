#include "roadDetectorPeyman.h"

using cv::CLAHE;

/* Constructor */ 
RoadDetectorPeyman::RoadDetectorPeyman()
{
	perc_look_old = 0.07;
	numOrientations = 4;
	orientations.resize(numOrientations);
	orientations[0] = 0.0;
	orientations[1] = CV_PI / 4.0;
	orientations[2] = CV_PI / 2.0;
	orientations[3] = CV_PI * 3 / 4.0;

	kernels.resize(numOrientations);
	float Sigma = 2 * CV_PI;
	float F = sqrt(2.0);
	for(int i = 0; i < numOrientations; i++)
	{
		kernels[i].resize(1);
		kernels[i][0].init(Sigma, F, orientations[i], 0);
	}
}

/* Distance between two points */
float distanceP(Point a, Point b)
{
	float difx = (a.x - b.x);
	float dify = (a.y - b.y);
	return sqrt(difx*difx  + dify*dify);
}

/* Check if point c is between point a and b */
bool isBetween(Point a, Point c, Point b)
{
	float dif = distanceP(a,c) + distanceP(c,b) - distanceP(a,b);
	return (dif <= 0.001 && dif >= -0.001);
}

/* Get in which point a line intersects another one */
bool get_line_intersection(Point p0, Point p1, Point p2, Point p3, Point& i)
{
	float s1_x, s1_y, s2_x, s2_y;
	s1_x = p1.x - p0.x;     s1_y = p1.y - p0.y;
	s2_x = p3.x - p2.x;     s2_y = p3.y - p2.y;

	float s, t;
	s = (-s1_y * (p0.x - p2.x) + s1_x * (p0.y - p2.y)) / (-s2_x * s1_y + s1_x * s2_y);
	t = ( s2_x * (p0.y - p2.y) - s2_y * (p0.x - p2.x)) / (-s2_x * s1_y + s1_x * s2_y);

	if (s >= 0 && s <= 1 && t >= 0 && t <= 1)
	{
		// Collision detected
		i.x = p0.x + (t * s1_x);
		i.y = p0.y + (t * s1_y);
		return 1;
	}

	return 0; // No collision
}

/* Apply Gabor Filter to the image */
void RoadDetectorPeyman::applyFilter(std::string dfilename, int dw, int dh)
{
	filename = dfilename;
	w = dw;
	h = dh;
	float middle_x = w / 2.0;
	float middle_y = h / 2.0;

	/* Create region of interest in the image */
	if(middle_y + 180 > h)
		margin_h = h * 0.1;
	else
		margin_h = h - (middle_y + 180);

	if(middle_x + 180 > w)
		margin_w = 0;
	else
		margin_w = w - (middle_x + 240);

	vp = Point(w/2 , h/2);

	/* Get image, resize and change to gray scale */
	image = imread(filename);
	resize(image, image, Size(w, h));
	outimage = image.clone();
	cvtColor(image, imageGray, CV_BGR2GRAY);

	// Start counting the time
	clock_t start;
	start = std::clock();
	int numScales = 1;

	responseOrientation.resize(numOrientations);

	/* Get the gabor energy response for each orientation */
	for(int i = 0; i < numOrientations ; i++)
	{
		vector< Mat > responseScale(numScales);
		for(int j = 0; j < numScales; j++)
			kernels[i][j].applyKernel(imageGray, responseScale[j]);

		Mat responseScaleSum = responseScale[0];
		for(int j = 0; j < numScales; j++)
			responseScaleSum = responseScaleSum + responseScale[j];

		responseOrientation[i] = responseScaleSum / numScales ; 
	}

	double duration = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
	cout << "gabor filter time " << duration << endl;
}

void RoadDetectorPeyman::applyFilter(Mat file, int dw, int dh)
{
	w = dw;
	h = dh;
	float middle_x = w / 2.0;
	float middle_y = h / 2.0;

	/* Create region of interest in the image */
	if(middle_y + 180 > h)
		margin_h = h * 0.1;
	else
		margin_h = h - (middle_y + 180);

	if(middle_x + 180 > w)
		margin_w = 0;
	else
		margin_w = w - (middle_x + 240);
	// margin_w = w * 0.2;
	// margin_h = h * 0.1;

	vp = Point(w/2 , h/2);

	/* Get image, resize and change to gray scale */
	image = file;
	resize(image, image, Size(w, h));
	cvtColor(image, imageGray, CV_BGR2GRAY);

	outimage = image.clone();

	// Start counting the time
	clock_t start;
	//cout << "Starting apply gabor filter" << endl;
	start = std::clock();
	int numScales = 1;

	responseOrientation.resize(numOrientations);

	/* Get the gabor energy response for each orientation */
	for(int i = 0; i < numOrientations ; i++)
	{
		vector< Mat > responseScale(numScales);
		for(int j = 0; j < numScales; j++)
			kernels[i][j].applyKernel(imageGray, responseScale[j]);

		Mat responseScaleSum = responseScale[0];
		for(int j = 0; j < numScales; j++)
			responseScaleSum = responseScaleSum + responseScale[j];

		responseOrientation[i] = responseScaleSum / numScales ; 
	}

	double duration = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
	cout << "gabor filter time " << duration << endl;
}

/* Calculates the dominant orientation for each pixel */
void RoadDetectorPeyman::calcOrientationConfiance()
{
	theta = Mat::zeros(h, w, CV_32F);
	conf = Mat::zeros(h, w, CV_32F);
	Mat s = Mat::zeros(h, w, CV_32F);
	vector<float*> temp(numOrientations);
	double dif1, dif2, x1 , y1, x2, y2; 
	double orientation[2];

	for(int i =0 ; i < h; i++)
	{
		for(int orientation = 0 ; orientation < numOrientations; orientation++){
			temp[orientation] = responseOrientation[orientation].ptr<float>(i);
		}

		for(int j = 0; j < w ; j++)
		{
			vector<float> response(numOrientations);
			for(int k = 0; k < numOrientations; k++){
				response[k] = temp[k][j];
			}

			vector<float> sortedResponse(response);
			std::sort(sortedResponse.begin(), sortedResponse.end(), greater<float>());
			conf.at<float>(i,j)  = 1 - (sortedResponse[1] / sortedResponse[0]);

			dif1 = abs(response[0]) - abs(response[2]);
			if(dif1 > 0)
				orientation[0] = orientations[0];
			else
				orientation[0] = orientations[2];

			dif2 = abs(response[1]) - abs(response[3]);
			if(dif2 > 0)
				orientation[1] = orientations[1];
			else
				orientation[1] = orientations[3];

			x1 = cos(orientation[0]) * abs(dif1);
			y1 = sin(orientation[0]) * abs(dif1);
			x2 = cos(orientation[1]) * abs(dif2);
			y2 = sin(orientation[1]) * abs(dif2);

			theta.at<float>(i,j) = (atan((y1 + y2) / (x1 + x2)));
		}   
	}

	// Normalize
	double minConf ;
	double maxConf ;
	minMaxLoc(conf, &minConf, &maxConf);

	for(int k =0 ; k < h; k++)
		for(int z = 0; z < w; z++){
			conf.at<float>(k,z) = float(conf.at<float>(k,z) - minConf) / float(maxConf - minConf);
		}
}

/* Returns in which quadrant belongs the angle */
int whichQuadrant(float angle)
{
	if(angle >= 0.0 && angle < CV_PI / 2.0)
		return 1;
	else if(angle >= CV_PI/2.0 && angle <= CV_PI)
		return 2;
	else if(angle > CV_PI && angle <= 3 * CV_PI / 2.0)
		return 3;
	else if(angle > 3* CV_PI/2.0 && angle <= 2*CV_PI)
		return 4;
	else
		return -1;
}


/* Change angle to be between 0 and 360 */
double constrainAngle(float x){
	float angleDeg = x * 180.0 / CV_PI;
	angleDeg = fmod(angleDeg,360);
	if (angleDeg < 0)
		angleDeg += 360;
	return angleDeg * CV_PI / 180.0;
}

/* Vanishing point voter */
void RoadDetectorPeyman::voter(const Point p, Mat& votes, Mat& dist_table, Point old_vp)
{
	float angle = theta.at<float>(p);
	angle = constrainAngle(angle);
	int quad = whichQuadrant(angle);

	/* Calculates the right direction of the ray */
	if(p.x >= w/2)
	{
		if(quad == 1)
			angle += CV_PI;
		else if(quad == 4)
			angle -= CV_PI;
	}
	else
	{
		if(quad == 2)
			angle += CV_PI;
		else if(quad == 3)
			angle -= CV_PI;
	}

	float cosAngle = cos(angle);
	float sinAngle = sin(angle);
	int diag = h + w;

	int x1  = p.x + int( diag * cosAngle);
	int y1 = p.y - int( diag * sinAngle);

	int step = 3;
	float xStep = (step * cosAngle);
	float yStep = (step * sinAngle);

	/* Checks which plane the ray intersects */
	Point plane(0,h);
	get_line_intersection(p, Point(x1,y1), Point(0,h), Point(w,h), plane);
	get_line_intersection(p, Point(x1,y1), Point(w,h), Point(w,0), plane);
	get_line_intersection(p, Point(x1,y1), Point(w,0), Point(0,0), plane);
	get_line_intersection(p, Point(x1,y1), Point(0,0), Point(0,h), plane);

	float sinTheta = abs(sinAngle);
	float distance;
	float distanceFunction; 
	double maxDistance = distanceP(p, plane);
	double variance = 0.25;

	float x = p.x + xStep;
	float y = p.y - yStep;

	while(y > margin_h && y < h - margin_h && x > margin_w && x < w - margin_w)
	{
		/* If not the first time calculating the vanishing point, 
		   only looks around the old one */
		if(old_vp.x != 0 && old_vp.y != 0)
		{

			if(y > old_vp.y - int(perc_look_old *h) && y < old_vp.y + int(perc_look_old *h) && x > old_vp.x - int(perc_look_old *w) && x < old_vp.x + int(perc_look_old *w)){
				votes.at<float>(y,x) += (sinTheta);
				distance = distanceP(p, Point(y,x)) / maxDistance;
				distanceFunction = exp(-(distance * distance) / (2.0 * variance ) );
				votes.at<float>(y,x) += distanceFunction * sinTheta ;
			}
			else
				votes.at<float>(y,x) = 0.0;
		}
		/* First time calculating the vanishing point */
		else
		{
			votes.at<float>(y,x) += (sinTheta);
			distance = distanceP(p, Point(y,x)) / maxDistance;
			distanceFunction = exp(-(distance * distance) / (2.0 * variance ) );
			votes.at<float>(y,x) += distanceFunction * sinTheta ;
		}
		x += xStep;
		y -= yStep;
	}
}

/* Find vanishing point based on vanishing point from previous frame */
Point RoadDetectorPeyman::findVanishingPoint(Point old_vp)
{
	votes = Mat::zeros(h, w, CV_32F);
	int maxH = 0.9 * h;
	Mat dist_table;

	// Start counting time
	clock_t start;
	double duration;
	start = std::clock();

	if(old_vp.x != 0 && old_vp.y != 0)
	{
		//rectangle(image, Point(old_vp.x - (perc_look_old * w), 
		//    old_vp.y - (perc_look_old *h)), Point(old_vp.x + (perc_look_old *w), old_vp.y + (perc_look_old * h)), Scalar(255, 0, 255));
	}

	for (int i = margin_h ; i < h - margin_h ; i++)
	{
		for (int j = margin_w  ; j < w - margin_w ; j++)
		{
			if(conf.at<float>(i,j) > 0.5)
				voter(Point(j,i), votes, dist_table, old_vp);
			//voter(Point(500,200), votes, dist_table);
		}
	}

	duration = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
	cout << "voter time " << duration << endl;

	double min_val, max_val;
	Point min_p, max_p;
	minMaxLoc(votes, &min_val, &max_val, &min_p, &max_p);

	circle(outimage, max_p, 6, Scalar(255, 0, 255), -1);
	vp = max_p;

	return max_p;
}

void RoadDetectorPeyman::drawConfidence()
{

	for(int i = 0 ; i < h ; i++)
	{
		for(int j = 0 ; j < w; j++)
		{
			if(conf.at<float>(i,j) > 0.5){
				outimage.at<cv::Vec3b>(i,j)[0] = 255;
				outimage.at<cv::Vec3b>(i,j)[1] = 0;
				outimage.at<cv::Vec3b>(i,j)[2] = 255;
			}
		}
	}
}

// Draw arrow in the direction of vanishing point
void drawDirectionArrow(Mat &img, const Point &p_start, double theta, 
	Scalar color = Scalar(40, 240, 160), double len = 200.0, double alpha = CV_PI / 6)
{
	Point p_end;
	p_end.x = p_start.x + int(len * cos(theta));
	p_end.y = p_start.y - int(len * sin(theta));
	line(img, p_start, p_end, color, 3);
	double len1 = len * 0.1;
	Point p_arrow;
	p_arrow.x = p_end.x - int(len1 * cos(theta - alpha));
	p_arrow.y = p_end.y + int(len1 * sin(theta - alpha));
	line(img, p_end, p_arrow, color, 3);
	p_arrow.x = p_end.x - int(len1 * cos(theta + alpha));
	p_arrow.y = p_end.y + int(len1 * sin(theta + alpha));
	line(img, p_end, p_arrow, color, 3);
}
void RoadDetectorPeyman::direction(Point van_point)
{
	double theta = atan2(h - van_point.y, van_point.x - w / 2);
	drawDirectionArrow(outimage, Point(w / 2, h), theta);
}

// Draw dominant orientation for pixels in the image
void RoadDetectorPeyman::drawOrientation(int grid, int lenLine)
{
	int maxH = 0.9 * h;
	for(int i = margin_h; i < h -margin_h ; i += grid)
	{
		for(int j = margin_w; j < w - margin_w; j += grid)
		{
			float angle = theta.at<float>(i,j);
			int x  = j + int( lenLine * cos(angle));
			int y = i - int( lenLine * sin(angle));
			line(outimage, Point(j,i), Point(x,y), Scalar(255,100,40), 2);
			circle(outimage, Point(j,i), 1, Scalar(255,255,0), -1);
		}
	}
}

// Get the OCR for each edge
float RoadDetectorPeyman::voteRoad(float angle, float thr, Point p)
{
	float angleRad = angle * CV_PI / 180.0;
	int step = 3;
	float xStep = (step * cos(angleRad));
	float yStep = (step * sin(angleRad));

	float x = p.x + xStep;
	float y = p.y - yStep;

	int totalPoints = 0;
	float total = 0.0;
	float dif = 0.0;
	while(y > 0 && y < h && x > 0 && x < w)
	{
		if(abs(theta.at<float>(y,x) -  (angleRad)) < thr || abs(theta.at<float>(y,x) + CV_PI -  (angleRad)) < thr)
			total += 1;
		x += xStep;
		y -= yStep;
		totalPoints += 1;
	}
	if(totalPoints == 0)
		return 0;
	else
		return float(total / totalPoints);
}

void RoadDetectorPeyman::drawEdge(float angle)
{
	int diag = h + w;
	int x2 = vp.x + int( diag * cos(angle));
	int y2 = vp.y - int( diag * sin(angle));
	line(outimage, Point(x2, y2), vp, Scalar(255,100,40), 2);
}

// Compute all the OCRs
float RoadDetectorPeyman::computeOCR(vector<float>& voteEdges, Point point, int initialAngle , float & sumTopOCR, float& median)
{
	int i = 0;
	int initialLeft = 200;
	int finalRight = -10;
	vector<float> tempVotes(voteEdges);
	float vote;
	float sum_vote;
	float last_angle = -1;

	if(initialAngle > 180 && initialAngle < 275)
		initialLeft = initialAngle + 20;
	else if(initialAngle >= 275 && initialAngle <= 360)
		finalRight = initialAngle - 360 - 20; 

	angle_left = 0.0;

	for(int angleLeft = initialLeft ; angleLeft < 275; angleLeft += 5)
	{
		vote = voteRoad(angleLeft, 0.15, point);
		if(median != 0.0 && vote >= median){
			if(angle_left == 0.0)
				angle_left = angleLeft* CV_PI / 180.0;
			//drawEdge(angleLeft* CV_PI / 180.0);
			if(last_angle == -1)
				last_angle = (angleLeft * CV_PI) / 180.0;
		}
		voteEdges[i] = vote;
		tempVotes[i] = vote;
		sum_vote += vote;
		i++;
	}
	for(int angleRight = -90; angleRight < finalRight; angleRight +=5)
	{
		vote = voteRoad(angleRight, 0.15, point);
		if(median != 0.0 && vote >= median){
			//drawEdge((angleRight + 360 )* CV_PI / 180.0);
			angle_right = (angleRight + 360 )* CV_PI / 180.0;
		}
		voteEdges[i] = vote;
		tempVotes[i] = vote;
		sum_vote += vote;
		i++;
	}

	int maxIndex = max_element(voteEdges.begin(), voteEdges.end()) - voteEdges.begin();
	float bestAngle = (initialLeft + (5 * maxIndex)) * CV_PI / 180.0;
	if(median == 0.0)
		last_angle = bestAngle;

	std::sort(tempVotes.begin(), tempVotes.end(), greater<float>());

	sumTopOCR = 0;
	for(int j = 0; j < 8; j++){
		sumTopOCR += tempVotes[j];
	}

	median = sum_vote / i;

	return last_angle;
}

// Algorithm to detect road
Point RoadDetectorPeyman::findRoad()
{
	clock_t start;
	double duration;

	//cout << "Starting Find Road" << endl;
	start = std::clock();

	float angle;
	int diag = h + w;
	vector<float> voteEdges(50,0);
	vector<float> sumOCR(diag,0);
	vector<Point> points(diag);
	vector<float> angles(diag);
	float sum = 0;
	float median = 0.0;
	float bestAngle = computeOCR(voteEdges, vp, 200, sum, median);
	//drawEdge(bestAngle);

	float bestAngleDeg = bestAngle * 180.0 / CV_PI;

	float xStep = cos(bestAngle);
	float yStep = sin(bestAngle);

	float x = vp.x;
	float y = vp.y;


	int i = 0;
	// while(y > margin_h && y < h - margin_h && x > margin_w && x < w - margin_w)
	// {
	// 	//circle(outimage, Point(x,y), 6, Scalar(255, 0, 0), -1);
	// 	vector<float> voteNewEdges(50,0);
	// 	float m = 0.0;
	// 	float bestAngleEdge = computeOCR(voteNewEdges, Point(x,y),bestAngleDeg, sumOCR[i], m);
	// 	//float edgeDeg = bestAngleEdge * CV_PI / 180.0;
	// 	x += xStep;
	// 	y -= yStep;
	// 	points[i] = Point(x,y);
	// 	angles[i] = bestAngleEdge;
	// 	i += 1;
	// }

	x = vp.x ;
	y = vp.y ;

	while(y > vp.y - 0.1*h && y < h - 0.1*h && x > 0.1*w && x < w - 0.1*w)
	{
		//circle(outimage, Point(x,y), 6, Scalar(255, 0, 0), -1);
		float m = 0.0;
		vector<float> voteNewEdges(50,0);
		float bestAngleEdge = computeOCR(voteNewEdges, Point(x,y), bestAngleDeg, sumOCR[i], m);
		// float edgeDeg = bestAngleEdge * CV_PI / 180.0;
		x -= xStep;
		y += yStep;
		points[i] = Point(x,y);
		angles[i] = bestAngleEdge;
		i += 1;
	}

	x = vp.x ;
	y = vp.y ;

	while(y > vp.y - 0.1*h && y < h - 0.1*h && x > 0.1*w && x < w - 0.1*w)
	{
		//circle(outimage, Point(x,y), 6, Scalar(255, 0, 0), -1);
		float m = 0.0;
		vector<float> voteNewEdges(50,0);
		float bestAngleEdge = computeOCR(voteNewEdges, Point(x,y),bestAngleDeg, sumOCR[i], m);
		// float edgeDeg = bestAngleEdge * CV_PI / 180.0;
		x -= xStep;
		y += yStep;
		points[i] = Point(x,y);
		angles[i] = bestAngleEdge;
		i -= 1;
	}
 
	int maxIndex = max_element(sumOCR.begin(), sumOCR.end()) - sumOCR.begin();
	vp = points[maxIndex];


	float second_best = computeOCR(voteEdges, vp, 200, sum, median);
	drawEdge(angle_left);
	drawEdge(angle_right);

	circle(outimage, vp, 6, Scalar(255, 0, 0), -1);


	int x1 = vp.x + int( diag * cos(angle_left));
	int y1 = vp.y - int( diag * sin(angle_left));

	int x2 = vp.x + int( diag * cos(angle_right));
	int y2 = vp.y - int( diag * sin(angle_right));

	triang1 = Point(x1,y1);
	triang2 = Point(x2,y2);

	// int npt[] = { 3 };
	// Point points_road[1][3];
	// points_road[0][0] = vp;
	// points_road[0][1] = Point(x1, y1);
	// points_road[0][2] = Point(x2,y2);

	// Point p1(0,h);
	// Point p2(w,h);
	// get_line_intersection(vp, Point(x1,y1), Point(0,h), Point(w,h), p1);
	// get_line_intersection(vp, Point(x2,y2), Point(0,h), Point(w,h), p2);

	// //equalizeRegion(p1, Point(p2.x, vp.y));

	// const Point* ppt[1] = { points_road[0] };

	// float alpha = 0.3;

	// Mat overlay;
	// outimage.copyTo(overlay);
	// fillPoly(overlay, ppt, npt, 1, Scalar( 255, 255, 0, 100 ), 8);
	// addWeighted(overlay, alpha, outimage, 1 - alpha, 0, outimage);

	duration = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
	cout << "finding road time " << duration << endl;

	return vp;
}

bool RoadDetectorPeyman::pointIn(Point p) {
	return (0<p.x && p.x<w && vp.y< p.y  && p.y< h);
}

static const Point shift[8]={Point(-1,0),Point(1,0),Point(0,-1),Point(0,1), Point(-1,1),Point(1,1),Point(1,-1),Point(1,1)};

float RoadDetectorPeyman::diffPixels(Point p, Point q, Mat img, int t0, int t1, int t2)
{

	// Vec3b p1_h = channelHSV[0].at<Vec3b>(p);
	// Vec3b p1_s = channelHSV[1].at<Vec3b>(p);
	// Vec3b p2_h = channelHSV[0].at<Vec3b>(q);
	// Vec3b p2_s = channelHSV[1].at<Vec3b>(q);

	// Vec3b dif_h = (p1_h - p2_h);
	// Vec3b dif_s = (p1_s - p2_s);
   
	// return sqrt((dif_h[0] * dif_h[0]) + (dif_s[1] * dif_s[1]) + (dif_s[2] * dif_s[2]));

	Vec3b p1 = img.at<Vec3b>(p);
	Vec3b p2 = img.at<Vec3b>(q);


	// return cv::norm(p1, p2, CV_L2); 
	//return sqrt((p1_h - p2_h) * (p1_h - p2_h)  + (p1_s - p2_s) * (p1_s - p2_s))
  //  return 0;


	float dif1 = abs(int(p1(0)) - int(p2(0))) ;
	float dif2 = abs(int(p1(1)) - int(p2(1))) ;
	float dif3 = abs(int(p1(2)) - int(p2(2))) ;
	
	// cout << "DIF 1 " << dif1 << endl;
	// cout << "DIF 2 " << dif2 << endl;
	// cout << "DIF 3 " << dif3 << endl;


	if(dif1 < t0 && dif2 < t1 && dif3 < t2 )
		return 1;
	else 
		return 0;
} 

bool RoadDetectorPeyman::isShadow(Point p, Mat img)
{
	Vec3b p1 = img.at<Vec3b>(p);
	//cout << "medium_bright: " << medium_bright << endl;
	// cout << "P1: " << p1 << endl;
	// cout << "This bright: " << int(p1(0)) << endl;
	if(p1[0]  < medium_bright - 0.5*medium_bright || p1[0]  > medium_bright + 0.5*medium_bright )
		return 1;
	else
		return 0;
}

void RoadDetectorPeyman::testShadow(Mat img)
{
	for(int i = 0; i < h ; i++)
	{
		for(int j = 0; j < w; j++)
		{
			Point p(j,i);
			if(isShadow(p, img))
				outimage.at<Vec3b>(p) = Vec3b(255,255,0); 
		}
	}
}

float sign (Point p1, Point p2, Point p3)
{
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

bool PointInTriangle (Point pt, Point v1, Point v2, Point v3)
{
    bool b1, b2, b3;

    b1 = sign(pt, v1, v2) < 0.0f;
    b2 = sign(pt, v2, v3) < 0.0f;
    b3 = sign(pt, v3, v1) < 0.0f;

    return ((b1 == b2) && (b2 == b3));
}

void RoadDetectorPeyman::regionGrow(Point seed, int t0, int t1, int t2, Mat img, Mat region, int useOrientation)
{

	if (!pointIn(seed)) return;            
	queue<Point> active;        
	active.push(seed);    // add the seed
	vector<float> firstdiff;
	float thr = 0.2 * w;
	while (!active.empty()) {

		Point p = active.front();
		active.pop();
		//outimage.at<Vec3b>(p) = Vec3b(255,255,0);     // set region
		region.at<int>(p) = 1;

		int pointVp = 0;

		int count = 0;
		float sum_direction = 0.0;
		for(int i =0; i < 8; i++)
		{
			for(int j = 1; j < 3; j++)
			{
				Point q = p + (shift[i] * j); 
				if(pointIn(q))
				{
					sum_direction += directionVp(q);
					count++;
				}
			}
		}
		float median_direction = sum_direction / count;

		bool one_in = 0;
		if(useOrientation == 1 || median_direction < thr){
			for(int i =0; i < 8; i++)
			{
				for(int j = 1; j < 3; j++)
				{
					Point q = p + (shift[i] * j);
					if(!pointIn(q) || region.at<float>(q) != 0)
						continue;

					if(isShadow(q, img))
					{
						region.at<float>(q) = 1;
						continue;
					}
					//float dif_pixels = diffPixels(p, q, img);

					//if(pointIn(q) && dif_pixels < T && region.at<float>(q) == 0 &&  pointVp > 0)
					if(pointIn(q) && diffPixels(p, q, img, t0, t1, t2) && region.at<float>(q) == 0 )
					{
						one_in = 1;
						break;
					}
				}
			}   
		}

		

		if(one_in){
			for(int i = 0; i < 8; i++)
			{
				for(int j = 1; j < 3; j++)
				{
					Point q = p + shift[i]; 
					if(!pointIn(q))
						continue;
					if(region.at<float>(q) == 0){
						if(PointInTriangle(q, triang1, vp, triang2)){
							active.push(q);
							region.at<int>(q) = 1;  
							outimage.at<Vec3b>(q) = Vec3b(100,100,10); 
						}
					}
				}

			}
		}

	}

}

float RoadDetectorPeyman::directionVp(Point p)
{
	float angle = theta.at<float>(p);
	int quad = whichQuadrant(angle);

	int step = 1;
	float xStep = (step * cos(angle));
	float yStep = (step * sin(angle));

	float x = p.x;
	float y = p.y;


	while(y > 0 && y < h && x > 0 && x < w)
	{

		if(int(y) == vp.y)
		{
			float distance = abs(vp.x - x);

			// float distance2;
			// if(yStep < 0)
			//     return distance;
			// else
			//     return abs(( p.y - vp.y ) / tan(angle));
			//cout << "Distance1 : " << distance << endl;
			//cout << "Distance2 : " << distance2 << endl;
			return distance;
		}
	  
		if(yStep < 0){
			x -= xStep;
			y += yStep;
		}
		else{
			x += xStep;
			y -= yStep;
		}
	}
	

	return w;

   // return abs(( p.y - vp.y ) / tan(angle));

}

void RoadDetectorPeyman::findLimits()
{
	Point left;
	Point right;
	for(int j = 0; j < w; j++)
	{
		for(int i = 0; i < h; i++)
		{
			if(outimage.at<Vec3b>(Point(j, i)) == Vec3b(255,255,0))
			{
				left = Point(j, i);
				break;
			}
		}
	}

	for(int j = w; j > 0 ; j--)
	{
		for(int i = 0; i < h; i++)
		{
			if(outimage.at<Vec3b>(Point(j, i)) == Vec3b(255,255,0))
			{
				right = Point(j, i);
				break;
			}
		}
	}

	line(outimage, vp, left, Scalar(255,100,40), 2);
	line(outimage, vp, right, Scalar(255,100,40), 2);
}

void RoadDetectorPeyman::findThr(Mat img, int& t0, int& t1, int& t2)
{
	int count = 0;
	float sum_dif0 = 0.0;
	float sum_dif1 = 0.0;
	float sum_dif2 = 0.0;
	int sum_bright = 0;
	for(int i = vp.y; i < h; i+=1)
	{
		Point p(w/2, i);
		for(int j = 0; j < 8; j++)
		{
			Point q = p + shift[j];   
			//float dif_pixels = diffPixels(p, q, img);

			// if(count > 5 && dif_pixels > (sum_dif/count )* 2)
			// 	continue;
			Vec3b p1 = img.at<Vec3b>(p);
			Vec3b p2 = img.at<Vec3b>(q);
			sum_bright += int(p1(0));

			sum_dif0 += abs(int(p1(0)) - int(p2(0))) ;
 			sum_dif1 += abs(int(p1(1)) - int(p2(1))) ;
 			sum_dif2 += abs(int(p1(2)) - int(p2(2))) ;

			count++;
		} 
	}
	t0 = int(sum_dif0 / count);
	medium_bright = sum_bright / count;
	t1 = int(sum_dif1 / count);
	t2 = int(sum_dif2 / count);

	cout << "T0 " << t0 << endl;
	cout << "T1 " << t1 << endl;
	cout << "T2 " << t2 << endl;

	if(t1 < 6.0)
		t1 = 6.0;
	if(t2 < 6.0)
		t2 = 6.0;
	if(t0 < 10.0)
		t0 = 10.0;

}

// void cumhist(int histogram[], int cumhistogram[])
// {
// 	cumhistogram[0] = histogram[0];
// 	for(int i = 1; i < 256; i++)
// 	{
// 		cumhistogram[i] = histogram[i] + cumhistogram[i-1];
// 	}
// }

// void imhist(Mat image, int histogram[], Point p1, Point p2)
// {

// 	// initialize all intensity values to 0
// 	for(int i = 0; i < 256; i++)
// 	{
// 		histogram[i] = 0;
// 	}

// 	// calculate the no of pixels for each intensity values
// 	for(int y = p2.y ; y < p1.y ; y++){
// 		for(int x = p1.x ; x < p2.x; x++)
// 		{
// 			histogram[(int)image.at<uchar>(y,x)]++;
// 		}
// 	}

// }

// void histDisplay(int histogram[], const char* name)
// {
//     int hist[256];
//     for(int i = 0; i < 256; i++)
//     {
//         hist[i]=histogram[i];
//     }
//     // draw the histograms
//     int hist_w = 512; int hist_h = 400;
//     int bin_w = cvRound((double) hist_w/256);
//  
//     Mat histImage(hist_h, hist_w, CV_8UC1, Scalar(255, 255, 255));
//  
//     // find the maximum intensity element from histogram
//     int max = hist[0];
//     for(int i = 1; i < 256; i++){
//         if(max < hist[i]){
//             max = hist[i];
//         }
//     }
//  
//     // normalize the histogram between 0 and histImage.rows
//  
//     for(int i = 0; i < 256; i++){
//         hist[i] = ((double)hist[i]/max)*histImage.rows;
//     }
//  
//  
//     // draw the intensity line for histogram
//     for(int i = 0; i < 256; i++)
//     {
//         line(histImage, Point(bin_w*(i), hist_h),
//                               Point(bin_w*(i), hist_h - hist[i]),
//              Scalar(0,0,0), 1, 8, 0);
//     }
//  
//     // display histogram
//     namedWindow(name, CV_WINDOW_AUTOSIZE);
//     imshow(name, histImage);
// }

// void RoadDetectorPeyman::equalizeRegion(Point p1, Point p2)
// {

// 	// Generate the histogram
// 	int histogram[256];
// 	imhist(imageGray, histogram, p1, p2);

// 	// Calculate the size of image
// 	int size = image.rows * image.cols;
// 	float alpha = 255.0/size;

// 	// Calculate the probability of each intensity
// 	float PrRk[256];
// 	for(int i = 0; i < 256; i++)
// 	{
// 		PrRk[i] = (double)histogram[i] / size;
// 	}

// 	// Generate cumulative frequency histogram
// 	int cumhistogram[256];
// 	cumhist(histogram,cumhistogram );

// 	// Scale the histogram
// 	int Sk[256];
// 	for(int i = 0; i < 256; i++)
// 	{
// 		Sk[i] = cvRound((double)cumhistogram[i] * alpha);
// 	}	

// 	// Generate the equlized histogram
//  	float PsSk[256];
// 	for(int i = 0; i < 256; i++)
//  	    PsSk[i] = 0;

//     for(int i = 0; i < 256; i++)
//         PsSk[Sk[i]] += PrRk[i];

// 	int final[256];
// 	for(int i = 0; i < 256; i++)
// 		final[i] = cvRound(PsSk[i]*255);

// 	// Generate the equlized image
// 	Mat new_image = imageGray.clone();

// 	for(int y = p2.y ; y < p1.y ; y++)
// 		for(int x = p1.x ; x < p2.x; x++)
// 		{
// 			new_image.at<uchar>(y,x) = saturate_cast<uchar>(Sk[imageGray.at<uchar>(y,x)]);
// 		}
	
// 	rectangle(imageGray, p1, p2, Scalar(0, 0, 0));
	
// 	//outimage = imageGray;
// 	namedWindow("Original Image");
// 	imshow("Original Image", imageGray);

// 	// Display the original Histogram
// 	histDisplay(histogram, "Original Histogram");

// // Display equilized image
// 	namedWindow("Equilized Image");
// 	imshow("Equilized Image",new_image);
// 	waitKey(0);
// 	imageGray = new_image;
	
// 	histDisplay(final, "Equilized Histogram");

// }


void RoadDetectorPeyman::roadDetection(float T)
{
	// Start counting the time
	clock_t start;
	start = std::clock();

	Point bottom(w/2, h);
	Point seed1 = (vp + bottom) * 0.5;

	//directionVp(Point(300,300));
	Mat imageDetection = image.clone();


	// ----- USING HSV ---- // 
	cvtColor(image, imageHSV, CV_BGR2HSV);
	split(imageHSV, channelHSV);
	channelHSV[2] = Mat(imageHSV.rows, imageHSV.cols, CV_8UC1, 280);//Set V
	merge(channelHSV, 3, imageHSV);
	cvtColor(imageHSV, imageDetection, CV_HSV2BGR);

	// USING YUV
	Mat imageYUV;
	cvtColor(image, imageYUV, CV_BGR2YUV);
	imageDetection = imageYUV;	


	// ----- USING YCbCr ---- //
	// Mat ycbcr;
	// cvtColor(image,ycbcr,CV_BGR2YCrCb);
	// Mat chan[3];
	// split(ycbcr,chan);
	// chan[0] = Mat(ycbcr.rows, ycbcr.cols, CV_8UC1, 150);
	// merge(chan, 3, ycbcr);
	// cvtColor(ycbcr, imageDetection, CV_YCrCb2BGR);

	//imageDetection = equalizeIntensity(imageDetection);
	//cvtColor(imageDetection, imageDetection, CV_BGR2GRAY); 
	//equalizeHist(imageDetection, imageDetection);
	//GaussianBlur( imageDetection, imageDetection, Size( 13,13 ), 0, 0 );
	//medianBlur(imageDetection,imageDetection, 7);

	//imageDetection = hsvImg;
   
	//medianBlur( imageGray, imageGray, 5 );
	//medianBlur( image, image, 5 );

	imshow("detection",imageDetection);
	// imshow("original",image);
	//waitKey(0);

	int t0, t1, t2;
	findThr(imageDetection, t0, t1, t2);

	Mat region = Mat::zeros(h, w, CV_32F);
	for(int i = vp.y + h * 0.2; i < (h - h * 0.2); i+=1)
	{
		regionGrow(Point(seed1.x, i), t0, t1, t2, imageDetection, region, T);
	}

	//testShadow(imageDetection);


	double duration = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
	cout << "road detection time " << duration << endl;


}
