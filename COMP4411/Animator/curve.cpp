#include "Curve.h"

#include <algorithm>
#include <functional>
#ifdef _DEBUG
#include <assert.h>
#endif // _DEBUG
#include <math.h>
#ifdef WIN32
#include <windows.h>
#endif // WIN32
#include <GL/gl.h>
#include <float.h>

#include "Curve.h"
#include "CurveEvaluator.h"

float Curve::s_fCtrlPtXEpsilon = 0.0001f;

Curve::Curve() :
	m_pceEvaluator(NULL),
	m_bWrap(false),
	m_bDirty(true),
	m_fMaxX(1.0f)
{
	init();
}

Curve::Curve(const float fMaxX, Point& point) :
	m_pceEvaluator(NULL),
	m_bWrap(false),
	m_bDirty(true),
	m_fMaxX(fMaxX)
{
	addControlPoint(point);
}

Curve::Curve(const float fMaxX, const float fStartYValue) :
	m_pceEvaluator(NULL),
	m_bWrap(false),
	m_bDirty(true),
	m_fMaxX(fMaxX)
{
	init(fStartYValue);
}

void Curve::init(const float fStartYValue /* = 0.0f */)
{
	Point p1(m_fMaxX * (1.0f / 3.0f), fStartYValue);
	Point p2(m_fMaxX * (2.0f / 3.0f), fStartYValue);
	m_ptvCtrlPts.push_back(p1);
	//sortControlPoints();
	double* tau1 = new double[2]{ 1 / 2,1/2 };
	double* angle1 = new double[2]{ 0 ,0};
	Point* innerPt1 = new Point[2]{ Point(m_fMaxX * (5.0f / 18.0f), fStartYValue), Point(m_fMaxX * (7.0f / 18.0f), fStartYValue) };
	taus.insert(std::pair<Point, double*>(m_ptvCtrlPts[0], tau1));
	angles.insert(std::pair<Point, double*>(m_ptvCtrlPts[0], angle1));
	innerPts.insert(std::pair<Point, Point*>(m_ptvCtrlPts[0], innerPt1));
	m_ptvCtrlPts.push_back(p2);
	//sortControlPoints();
	double* tau2 = new double[2]{ 1 / 2 ,1/2};
	double* angle2 = new double[2]{ 0 ,0};
	Point* innerPt2 = new Point[2]{ Point(m_fMaxX * (11.0f / 18.0f), fStartYValue), Point(m_fMaxX * (13.0f / 18.0f), fStartYValue) };
	taus.insert(std::pair<Point, double*>(m_ptvCtrlPts[1], tau2));
	angles.insert(std::pair<Point, double*>(m_ptvCtrlPts[1], angle2));
	innerPts.insert(std::pair<Point, Point*>(m_ptvCtrlPts[1], innerPt2));
	//m_ptvCtrlPts.push_back(Point(m_fMaxX * (1.0f / 3.0f), fStartYValue));
	//m_ptvCtrlPts.push_back(Point(m_fMaxX * (2.0f / 3.0f), fStartYValue));
}

void Curve::maxX(const float fNewMaxX)
{
	m_fMaxX = fNewMaxX;

	for (int i = 0; i < m_ptvCtrlPts.size(); ++i) {
		if (m_ptvCtrlPts[i].x > m_fMaxX) {
			m_ptvCtrlPts[i].x = m_fMaxX;
		}
	}

	m_bDirty = true;
}

Curve::Curve(std::istream& isInputStream)
{
	fromStream(isInputStream);
}

void Curve::toStream(std::ostream & output_stream) const
{
	output_stream << m_ptvCtrlPts.size() << std::endl;

	for (std::vector<Point>::const_iterator control_point_iterator = m_ptvCtrlPts.begin(); control_point_iterator != m_ptvCtrlPts.end(); ++control_point_iterator) {
		output_stream << *control_point_iterator;
	}

	output_stream << m_fMaxX << std::endl;

	output_stream << m_bWrap << std::endl;
}

void Curve::fromStream(std::istream& isInputStream)
{
	int iCtrlPtCount;

	isInputStream >> iCtrlPtCount;

	m_ptvCtrlPts.resize(iCtrlPtCount);

	for (int iCtrlPt = 0; iCtrlPt < iCtrlPtCount; ++iCtrlPt) {
		isInputStream >> m_ptvCtrlPts[iCtrlPt];
	}

	isInputStream >> m_fMaxX;

	isInputStream >> m_bWrap;

	m_bDirty = true;
}

void Curve::wrap(bool bWrap)
{
	m_bWrap = bWrap;
	m_bDirty = true;
}

bool Curve::wrap() const
{
	return m_bWrap;
}

float Curve::evaluateCurveAt(const float x) const
{
	reevaluate();
	
	float value = 0.0f;

	if (m_ptvEvaluatedCurvePts.size() == 1)
		return m_ptvEvaluatedCurvePts[0].y;

	if (m_ptvEvaluatedCurvePts.size() > 1) {
		std::vector<Point>::iterator first_point = m_ptvEvaluatedCurvePts.begin();
		std::vector<Point>::iterator last_point = m_ptvEvaluatedCurvePts.end() - 1;

		bool evaluate_point_to_left_of_range = (first_point->x > x);
		bool evaluate_point_to_right_of_range = (last_point->x < x);

		if (evaluate_point_to_left_of_range) {
			value = first_point->y;
		}
		else if (evaluate_point_to_right_of_range) {
			value = last_point->y;
		}
		else {
			std::vector<Point>::iterator point_one_iterator = first_point;

			while ((point_one_iterator + 1)->x < x) {
				++point_one_iterator;
#ifdef _DEBUG
				assert(point_one_iterator != last_point);
#endif // _DEBUG
			}

			std::vector<Point>::iterator point_two_iterator = point_one_iterator + 1;
			
#ifdef _DEBUG
			assert(point_one_iterator != m_ptvEvaluatedCurvePts.end());
			assert(point_two_iterator != m_ptvEvaluatedCurvePts.end());
#endif // _DEBUG

			Point point_one = *point_one_iterator;
			Point point_two = *point_two_iterator;

			if (point_one.x == point_two.x)
				value = point_one.y;
			else {
				float slope = (point_two.y - point_one.y) / (point_two.x - point_one.x);
				value = (x - point_one.x) * slope + point_one.y;
			}
		}
	}

	return value;
}

void Curve::scaleX(const float fScale)
{
	for (std::vector<Point>::iterator control_point_iterator = m_ptvCtrlPts.begin(); 
	     control_point_iterator != m_ptvCtrlPts.end(); 
		 ++control_point_iterator) {
		control_point_iterator->x *= fScale;
	}
	m_fMaxX *= fScale;
	m_bDirty = true;
}

int getIndex(std::vector<Point> pts, Point pt) {
	std::vector<Point>::iterator i = std::find(pts.begin(), pts.end(), pt);
	if (i != pts.end()) {
		int index = i - pts.begin();
		return index;
	}
	else {
		return 0;
	}
}

void Curve::addControlPoint(Point& point)
{
	m_ptvCtrlPts.push_back(point);
	sortControlPoints();
	double* tau = new double[2]{ VAL(CR_TENSION),VAL(CR_TENSION) };
	double* angle = new double[2]{ 0,0 };
	int iCtrlPt = getIndex(m_ptvCtrlPts, point);
	Point cqian = iCtrlPt - 1 < 0 ? m_ptvCtrlPts[0] : m_ptvCtrlPts[iCtrlPt - 1];
	Point chou = iCtrlPt + 1 >= m_ptvCtrlPts.size() ? m_ptvCtrlPts[m_ptvCtrlPts.size() - 1] : m_ptvCtrlPts[iCtrlPt + 1];
	Point zqian = -tau[0] / 3 * (chou - cqian);
	Point zhou = tau[1] / 3 * (chou - cqian);
	// zqian.rotate(angle[0]);
	// zhou.rotate(angle[1]);
	Point qian = point + zqian, hou = point + zhou;
	Point* neigh = new Point[2]{ qian, hou };
	taus.insert(std::pair<Point, double*>(m_ptvCtrlPts[iCtrlPt], tau));
	angles.insert(std::pair<Point, double*>(m_ptvCtrlPts[iCtrlPt], angle));
	innerPts.insert(std::pair<Point, Point*>(point, neigh));
	m_bDirty = true;
}

void Curve::removeControlPoint(const int iCtrlPt)
{
	if (iCtrlPt < m_ptvCtrlPts.size() && m_ptvCtrlPts.size() > 2) {
		std::vector<Point>::iterator i = m_ptvCtrlPts.begin() + iCtrlPt;
		delete[] taus.find(*i)->second; delete[] angles.find(*i)->second; delete[] innerPts.find(*i)->second;
		taus.erase(*i); angles.erase(*i); innerPts.erase(*i);
		m_ptvCtrlPts.erase(i);
		m_bDirty = true;
	}
}

/** This fxn allows removal of all ctrl points (not just down to 2) **/
void Curve::removeControlPoint2(const int iCtrlPt)
{
	if (iCtrlPt < m_ptvCtrlPts.size()) {
		std::vector<Point>::iterator i = m_ptvCtrlPts.begin() + iCtrlPt;
		delete[] taus.find(*i)->second; delete[] angles.find(*i)->second;
		taus.erase(*i); angles.erase(*i);
		m_ptvCtrlPts.erase(i);
		m_bDirty = true;
	}
}

int Curve::getClosestControlPoint(const Point& point, Point& ptCtrlPt) const
{
	reevaluate();

	int iMinDistPt = 0;
	float fMinDistSquared = FLT_MAX;

	for (int i = 0; i < m_ptvCtrlPts.size(); ++i) { 
		float delta_x = (m_ptvCtrlPts[i].x - point.x);
		float delta_y = (m_ptvCtrlPts[i].y - point.y);

		float fDistSquared = delta_x * delta_x + delta_y * delta_y;

		if (fDistSquared < fMinDistSquared) {
			iMinDistPt = i;
			fMinDistSquared = fDistSquared;
			ptCtrlPt = m_ptvCtrlPts[i];
		}
	}

	return iMinDistPt;
}

Point Curve::getClosestInnerPoint(const Point& point, Point& ptInnerPt) const
{
	Point Ctrl;
	float fMinDistSquared = FLT_MAX;

	for (std::map<Point, Point*>::iterator i = innerPts.begin(); i != innerPts.end(); ++i) {
		for (int j = 0; j < 2; ++j) {
			float delta_x1 = (i->second[j].x - point.x);
			float delta_y1 = (i->second[j].y - point.y);

			float fDistSquared1 = delta_x1 * delta_x1 + delta_y1 * delta_y1;

			if (fDistSquared1 < fMinDistSquared) {
				Ctrl = i->first;
				fMinDistSquared = fDistSquared1;
				ptInnerPt = i->second[j];
			}
		}
	}

	return Ctrl;
}

void Curve::getClosestPoint(const Point& pt, Point& ptClosestPt) const
{
	reevaluate();

	ptClosestPt = Point(pt.x, evaluateCurveAt(pt.x));
}

float Curve::getDistanceToCurve(const Point& point) const
{
	reevaluate();

	Point ptEvaluatedPt(point.x, evaluateCurveAt(point.x));

	float delta_x = point.x - ptEvaluatedPt.x;
	float delta_y = point.y - ptEvaluatedPt.y;
	float distance = sqrt(delta_x * delta_x + delta_y * delta_y);

	return distance;
}

int Curve::controlPointCount() const
{
	return m_ptvCtrlPts.size();
}

int Curve::segmentCount() const
{
	reevaluate();

	int iEvaluatedPtCount = m_ptvEvaluatedCurvePts.size();

	if (iEvaluatedPtCount == 0) {
		return 0;
	}
	else {
		return iEvaluatedPtCount - 1;
	}
}

void Curve::moveControlPoint(const int iCtrlPt, const Point& ptNewPt)
{
	int iCtrlPtCount = m_ptvCtrlPts.size();

#ifdef _DEBUG
	assert(iCtrlPt < iCtrlPtCount);
#endif // _DEBUG

	if (iCtrlPt < iCtrlPtCount) {
		Point oldpt = m_ptvCtrlPts[iCtrlPt];
		m_ptvCtrlPts[iCtrlPt] = ptNewPt;

		// adjust the x value so that it falls within the
		// points right before and after it
		if (iCtrlPt > 0) {
			if (m_ptvCtrlPts[iCtrlPt].x < m_ptvCtrlPts[iCtrlPt - 1].x + s_fCtrlPtXEpsilon)
				m_ptvCtrlPts[iCtrlPt].x = m_ptvCtrlPts[iCtrlPt - 1].x + s_fCtrlPtXEpsilon;
		}
		if (iCtrlPt < iCtrlPtCount - 1) {
			if (m_ptvCtrlPts[iCtrlPt].x > m_ptvCtrlPts[iCtrlPt + 1].x - s_fCtrlPtXEpsilon)
				m_ptvCtrlPts[iCtrlPt].x = m_ptvCtrlPts[iCtrlPt + 1].x - s_fCtrlPtXEpsilon;
		}

		double* t = taus.find(oldpt)->second, *a = angles.find(oldpt)->second;
		taus.erase(oldpt); angles.erase(oldpt);
		taus.insert(std::pair<Point, double*>(m_ptvCtrlPts[iCtrlPt], t));
		angles.insert(std::pair<Point, double*>(m_ptvCtrlPts[iCtrlPt], a));
	}


	m_bDirty = true;
}

void Curve::moveControlPoints(const std::vector<int>& ivCtrlPts, const Point& ptOffset,
							  const float fMinY, const float fMaxY)
{
	int iCtrlPtCount = m_ptvCtrlPts.size();

#ifdef _DEBUG
	for (int iCheck = 0; iCheck < ivCtrlPts.size(); ++iCheck) {
		assert(ivCtrlPts[iCheck] < iCtrlPtCount);
	}
#endif // _DEBUG

	Point ptActualOffset = ptOffset;
	int i;

	// make sure the will be moved points will not run over other
	// static control points. Also limit the y value.
	for (i = 0; i < ivCtrlPts.size(); ++i) {
		int iCtrlPt = ivCtrlPts[i];

		if (m_ptvCtrlPts[iCtrlPt].y + ptActualOffset.y > fMaxY)
			ptActualOffset.y = fMaxY - m_ptvCtrlPts[iCtrlPt].y;
		if (m_ptvCtrlPts[iCtrlPt].y + ptActualOffset.y < fMinY)
			ptActualOffset.y = fMinY - m_ptvCtrlPts[iCtrlPt].y;

		if (iCtrlPt > 0) {
			if (std::find(ivCtrlPts.begin(), ivCtrlPts.end(), iCtrlPt - 1) == ivCtrlPts.end()) {
				if (m_ptvCtrlPts[iCtrlPt].x + ptActualOffset.x < m_ptvCtrlPts[iCtrlPt - 1].x + s_fCtrlPtXEpsilon)
					ptActualOffset.x = m_ptvCtrlPts[iCtrlPt - 1].x + s_fCtrlPtXEpsilon - m_ptvCtrlPts[iCtrlPt].x;
			}
		}
		else { // iCtrlPt == 0
			if (m_ptvCtrlPts[iCtrlPt].x + ptActualOffset.x < 0.0f)
				ptActualOffset.x = -m_ptvCtrlPts[iCtrlPt].x;
		}

		if (iCtrlPt < iCtrlPtCount - 1) {
			if (std::find(ivCtrlPts.begin(), ivCtrlPts.end(), iCtrlPt + 1) == ivCtrlPts.end()) {
				if (m_ptvCtrlPts[iCtrlPt].x + ptActualOffset.x > m_ptvCtrlPts[iCtrlPt + 1].x - s_fCtrlPtXEpsilon)
					ptActualOffset.x = m_ptvCtrlPts[iCtrlPt + 1].x - s_fCtrlPtXEpsilon - m_ptvCtrlPts[iCtrlPt].x;
			}
		}
		else { // iCtrlPt == iCtrlPtCount - 1
			if (m_ptvCtrlPts[iCtrlPt].x + ptActualOffset.x > m_fMaxX)
				ptActualOffset.x = m_fMaxX - m_ptvCtrlPts[iCtrlPt].x;
		}
	}

	// move the control points
	for (i = 0; i < ivCtrlPts.size(); ++i) {
		int iCtrlPt = ivCtrlPts[i];
		Point oldpt = m_ptvCtrlPts[iCtrlPt];
		m_ptvCtrlPts[iCtrlPt].x += ptActualOffset.x;
		m_ptvCtrlPts[iCtrlPt].y += ptActualOffset.y;

		double* t = taus.find(oldpt)->second, * a = angles.find(oldpt)->second;
		Point* n = innerPts.find(oldpt)->second; delete[] n;
		Point cqian = iCtrlPt - 1 < 0 ? m_ptvCtrlPts[0] : m_ptvCtrlPts[iCtrlPt - 1];
		Point chou = iCtrlPt + 1 >= m_ptvCtrlPts.size() ? m_ptvCtrlPts[m_ptvCtrlPts.size() - 1] : m_ptvCtrlPts[iCtrlPt + 1];
		Point zqian = -t[0] / 3 * (chou - cqian);
		Point zhou = t[1] / 3 * (chou - cqian);
		zqian.rotate(a[0]);
		zhou.rotate(a[1]);
		Point qian = m_ptvCtrlPts[iCtrlPt] + zqian, hou = m_ptvCtrlPts[iCtrlPt] + zhou;
		Point* neigh = new Point[2]{ qian, hou };
		taus.erase(oldpt); angles.erase(oldpt); innerPts.erase(oldpt);
		taus.insert(std::pair<Point, double*>(m_ptvCtrlPts[iCtrlPt], t));
		angles.insert(std::pair<Point, double*>(m_ptvCtrlPts[iCtrlPt], a));
		innerPts.insert(std::pair<Point, Point*>(m_ptvCtrlPts[iCtrlPt], neigh));
	}

	m_bDirty = true;
}

void Curve::moveInnerPoints(const Point Ctrl, Point& currInnerPt, const Point& ptOffset,
	const float fMinY, const float fMaxY)
{
	Point ptActualOffset = ptOffset;

	// move the inner points
	Point oldpt = currInnerPt;
	currInnerPt.x += ptActualOffset.x;
	currInnerPt.y += ptActualOffset.y;

	int indx = getIndex(m_ptvCtrlPts, Ctrl);
	double* t = taus.find(Ctrl)->second, * a = angles.find(Ctrl)->second;
	Point* n = innerPts.find(Ctrl)->second; int i;
	if (n[0].distance(oldpt) < 4) {
		if (n[0].distance(oldpt) > 0.005) std::cout << n[0].distance(oldpt) << std::endl;
		i = 0;
	}
	else if (n[1].distance(oldpt) < 4) {
		if (n[1].distance(oldpt) > 0.005) std::cout << n[1].distance(oldpt) << std::endl;
		i = 1;
	}
	else {
		std::cerr << "cannot find corresponding inner point" << std::endl;
		std::cout << n[0] << n[1] << std::endl;
	}

	n[i] = currInnerPt;
	Point qian = indx == 0 ? m_ptvCtrlPts[0] : m_ptvCtrlPts[indx - 1];
	Point hou = indx == m_ptvCtrlPts.size() - 1 ? m_ptvCtrlPts[m_ptvCtrlPts.size() - 1] : m_ptvCtrlPts[indx + 1];
	Point diff = i == 0 ? qian - hou : hou - qian;
	Point newdiff = currInnerPt - Ctrl;
	float L = diff.distance(Point(0, 0));
	float newL = currInnerPt.distance(Ctrl);
	t[i] = newL / L * 3;
	double an = atan2(newdiff.y, newdiff.x) - atan2(diff.y, diff.x);
	a[i] = an; // can we do nothing on "an"?

	m_bDirty = true;

	reevaluate();
}

void Curve::drawCurve() const
{
	reevaluate();

	drawEvaluatedCurveSegments();
}

void Curve::drawEvaluatedCurveSegments() const
{
	reevaluate();

	glBegin(GL_LINE_STRIP);

		for (std::vector<Point>::const_iterator it = m_ptvEvaluatedCurvePts.begin(); 
			it != m_ptvEvaluatedCurvePts.end(); 
			++it) {
			glVertex2f(it->x, it->y);
		}

	glEnd();
}

void Curve::drawControlPoint(int iCtrlPt) const
{
	reevaluate();

	double fPointSize;
	glGetDoublev(GL_POINT_SIZE, &fPointSize);
	glPointSize(7.0);

	glColor3d(1,0,0);
	glBegin(GL_POINTS);
		glVertex2f(m_ptvCtrlPts[iCtrlPt].x, m_ptvCtrlPts[iCtrlPt].y);
	glEnd();

	glPointSize(fPointSize);
}

void Curve::drawInnerPoint(int iCtrlPt) const
{
	//reevaluate();

	double fPointSize;
	glGetDoublev(GL_POINT_SIZE, &fPointSize);
	glPointSize(7.0);

	glColor3d(0, 1, 0);
	glBegin(GL_POINTS);
	Point cpt = m_ptvCtrlPts[iCtrlPt];
	double* tau = taus.find(cpt)->second;
	double* angle = angles.find(cpt)->second;
	if (prev_tau != VAL(CR_TENSION)) {
		if (tau[0] == prev_tau && angle[0] == 0) tau[0] = VAL(CR_TENSION);
		if (tau[1] == prev_tau && angle[1] == 0) tau[1] = VAL(CR_TENSION);
	}
	Point cqian = iCtrlPt - 1 < 0 ? m_ptvCtrlPts[0] : m_ptvCtrlPts[iCtrlPt - 1];
	Point chou = iCtrlPt + 1 >= m_ptvCtrlPts.size() ? m_ptvCtrlPts[m_ptvCtrlPts.size()-1]: m_ptvCtrlPts[iCtrlPt + 1];
	Point zqian = - tau[0] / 3 * (chou - cqian);
	Point zhou = tau[1] / 3 * (chou - cqian);
	zqian.rotate(angle[0]);
	zhou.rotate(angle[1]);
	Point qian = cpt + zqian, hou = cpt + zhou;
	Point* neigh = innerPts.find(cpt)->second;
	neigh[0] = qian; neigh[1] = hou;
	glVertex2f(qian.x,qian.y);
	glVertex2f(hou.x,hou.y);
	glEnd();

	glPointSize(fPointSize);
}

void Curve::drawInnerPoints() const
{
	//reevaluate();

	double fPointSize;
	glGetDoublev(GL_POINT_SIZE, &fPointSize);
	glPointSize(7.0);

	glColor3d(1,1,1);
	glBegin(GL_POINTS);
		for (std::vector<Point>::const_iterator kit = m_ptvCtrlPts.begin(); 
			kit != m_ptvCtrlPts.end(); 
			++kit) {
			Point cpt = *kit;
			int iCtrlPt = getIndex(m_ptvCtrlPts, cpt);
			double* tau = taus.find(cpt)->second;
			double* angle = angles.find(cpt)->second;
			if (prev_tau != VAL(CR_TENSION)) {
				if (tau[0] == prev_tau && angle[0] == 0) tau[0] = VAL(CR_TENSION);
				if (tau[1] == prev_tau && angle[1] == 0) tau[1] = VAL(CR_TENSION);
			}
			Point cqian = iCtrlPt - 1 < 0 ? m_ptvCtrlPts[0] : m_ptvCtrlPts[iCtrlPt - 1];
			Point chou = iCtrlPt + 1 >= m_ptvCtrlPts.size() ? m_ptvCtrlPts[m_ptvCtrlPts.size() - 1] : m_ptvCtrlPts[iCtrlPt + 1];
			Point zqian = -tau[0] / 3 * (chou - cqian);
			Point zhou = tau[1] / 3 * (chou - cqian);
			zqian.rotate(angle[0]);
			zhou.rotate(angle[1]);
			Point qian = cpt + zqian, hou = cpt + zhou;
			Point* neigh = new Point[2]{ qian, hou };
			innerPts.insert(std::pair<Point, Point*>(cpt, neigh));
			glVertex2f(qian.x, qian.y);
			glVertex2f(hou.x, hou.y);
		}
	glEnd();

	glPointSize(fPointSize);
}

void Curve::drawControlPoints() const
{
	reevaluate();

	double fPointSize;
	glGetDoublev(GL_POINT_SIZE, &fPointSize);
	glPointSize(7.0);

	glColor3d(1, 1, 1);
	glBegin(GL_POINTS);
	for (std::vector<Point>::const_iterator kit = m_ptvCtrlPts.begin();
		kit != m_ptvCtrlPts.end();
		++kit) {
		glVertex2f(kit->x, kit->y);
	}
	glEnd();

	glPointSize(fPointSize);
}

void Curve::sortControlPoints() const
{
	std::sort(m_ptvCtrlPts.begin(),
		m_ptvCtrlPts.end(),
		PointSmallerXCompare());
}

void Curve::reevaluate() const
{
	if (m_bDirty) {
		if (m_pceEvaluator) {
			m_pceEvaluator->evaluateCurve(m_ptvCtrlPts, 
				m_ptvEvaluatedCurvePts, 
				m_fMaxX, 
				m_bWrap, innerPts);

			std::sort(m_ptvEvaluatedCurvePts.begin(),
				m_ptvEvaluatedCurvePts.end(),
				PointSmallerXCompare());

			m_bDirty = false;
		}
	}
}

void Curve::invalidate() const
{
	m_bDirty = true;
}

std::ostream& operator<<(std::ostream& output_stream, const Curve & curve_data)
{
	curve_data.toStream(output_stream);
	return output_stream;
}

std::istream& operator>>(std::istream& isInputStream, Curve & curve_data)
{
	curve_data.fromStream(isInputStream);
	return isInputStream;
}

