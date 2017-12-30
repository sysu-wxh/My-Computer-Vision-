#include "Map.h"

void Map::storeCluster(double center1, double center2, double center3,    // center
	double param1, double param2, double param3,     // paramter
	int color1, int color2, int color3,  // color
	VERTEX3D vertex) {

	double precision = precision_;
	double normVec = sqrt(param1 * param1 + param2 * param2 + 1.0);
	double normParam1 = param1 / normVec, normParam2 = param2 / normVec, normParam3 = -1.0 / normVec;
	int nParam1 = normParam1 / precision, nParam2 = normParam2 / precision, nParam3 = normParam3 / precision;
	//std::cout << precision << std::endl;
	//std::cout << nParam1 << " " << nParam2 << " " << nParam3 << std::endl;

	int index = findCoplane(nParam1, nParam2, nParam3);
	//index = -1;
	//cout << index << endl;&& center2 > -1.5
	if (index != -1 && center2 > -1.5) {
		cluster_[index].addSegment(center1, center2, center3, param1, param2, param3, color1, color2, color3, vertex);
	}
	else {
		clusterCnt_++;
		cluster_.push_back(Cluster::Cluster(center1, center2, center3, param1, param2, param3, color1, color2, color3,
			nParam1, nParam2, nParam3, vertex));
	}
}

void Map::outputFile(string filePath, int mode) {
	ofstream output(filePath.c_str(), ios::out);
	for (int i = 0; i < clusterCnt_; ++i) {
		int r, g, b;
		int vertex3DSize = cluster_[i].getVertex3DSize();
		for (int t = 0; t < vertex3DSize; ++t) {
			int vertexNum = cluster_[i].getVertexNum(t);
			if (vertexNum == 0)
				continue;
			else {
				cluster_[i].getAverageColor(r, g, b);
				output << vertexNum << " ";
				for (int j = 0; j < vertexNum; ++j)
					output << cluster_[i].getVertexX(t, j) << " "
					<< cluster_[i].getVertexY(t, j) << " "
					<< cluster_[i].getVertexZ(t, j) << " ";
				output << r << " " << g << " " << b << endl;
			}
		}
	}
	output.close();
}

int Map::findCoplane(int c1, int c2, int c3) {
	for (int i = 0; i < clusterCnt_; ++i) {
		int currentC1, currentC2, currentC3;
		if (cluster_[i].checkCoplane(c1, c2, c3))
			return i;
	}
	return -1;
}

bool Map::Cluster::checkCoplane(int c1, int c2, int c3) {
	return (norm1_ == c1 && norm2_ == c2 && norm3_ == c3);
}