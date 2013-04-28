package cn.songshan99.FootprintParser;

import java.util.ArrayList;
import java.util.HashMap;

import static java.lang.Math.*;
import static android.util.FloatMath.*;
import android.graphics.Path;
import android.graphics.RectF;

public class ICFootprint {

	private ArrayList<PinOrPadOrDraftLine> mListPinOrPad;
	private ArrayList<PinOrPadOrDraftLine> mListDraftLine;
	public Mark mMark,mTextLoc;
	public String mDesc, mName, mValue;
	
	public int flags;
	public ICText mICText;
	
	private static final HashMap<String, Integer> strFlagMap = new HashMap<String, Integer>(){
        {
        	put("pin",	0x0001);
        	put("via",0x0002);
        	put("found",0x0004);
        	put("hole",0x0008);
        	put("nopaste",0x0008);
        	put("rat",0x0010);
        	put("pininpoly",0x0010);
        	put("clearpoly",0x0010);
        	put("hidename",0x0010);
        	put("showname",0x0020);
        	put("clearline",0x0020);
        	put("fullpoly",0x0020);
        	put("selected",0x0040);
        	put("onsolder",0x0080);
        	put("auto",0x0080);
        	put("square",0x0100);
        	put("rubberend",0x0200);
        	put("warn",0x0200);
        	put("usetherm",0x0400);
        	put("octagon",0x0800);
        	put("drc",0x1000);
        	put("lock",0x2000);
        	put("edge2",0x4000);
        	put("marker",0x8000);
        	put("connected",0x10000);
        }
    };
    
	public String getmValue() {
		return mValue;
	}
	public void setmValue(String mValue) {
		this.mValue = mValue;
	}
	public String getmDesc() {
		return mDesc;
	}
	public void setmDesc(String mDesc) {
		this.mDesc = mDesc;
	}
	public String getmName() {
		return mName;
	}
	public void setmName(String mName) {
		this.mName = mName;
	}
	public int getFlags() {
		return flags;
	}
	public void setFlags(int flags) {
		this.flags = flags;
	}
	public ICText getmICText() {
		return mICText;
	}
	public void setmICText(ICText mICText) {
		this.mICText = mICText;
	}
	public ArrayList<PinOrPadOrDraftLine> getmListPinOrPad() {
		return mListPinOrPad;
	}
	public ArrayList<PinOrPadOrDraftLine> getmListDraftLine() {
		return mListDraftLine;
	}
	public Mark getmMark() {
		return mMark;
	}
	
	public void setmMark(Mark mMark) {
		this.mMark = mMark;
	}
	
	public ICFootprint() {
		mListPinOrPad = new ArrayList<PinOrPadOrDraftLine>();
		mListDraftLine = new ArrayList<PinOrPadOrDraftLine>();
	}
	
	public ICFootprint(int icflags, ICText ictext) {
		flags = icflags;
		mICText = ictext;
		mListPinOrPad = new ArrayList<PinOrPadOrDraftLine>();
		mListDraftLine = new ArrayList<PinOrPadOrDraftLine>();
	}
	
	public void addPin(Pin pin){
		mListPinOrPad.add(pin);
	}
	
	public void addPad(Pad pad){
		mListPinOrPad.add(pad);
	}
	
	public void addLine(ElementLine line){
		mListDraftLine.add(line);
	}
	
	public void addArc(ElementArc arc){
		mListDraftLine.add(arc);
	}
	
	public void setMark(Mark mk){
		mMark = mk;
	}
	
	public void addPinOrPadOrDraftLine(PinOrPadOrDraftLine obj){
		switch(obj.getType()){
		case PinOrPadOrDraftLine.TYPE_PIN:
			addPin((Pin)obj);
			break;
		case PinOrPadOrDraftLine.TYPE_PAD:
			addPad((Pad)obj);
			break;
		case PinOrPadOrDraftLine.TYPE_LINE:
			addLine((ElementLine)obj);
			break;
		case PinOrPadOrDraftLine.TYPE_ARC:
			addArc((ElementArc)obj);
			break;
		case PinOrPadOrDraftLine.TYPE_MARK:
			setmMark((Mark)obj);
		}
	}
	
	private RectF calculatePinPadOverallBoundRectangle(){
		//this function returns null of no pin or pad exists
		RectF result = null;
		for(PinOrPadOrDraftLine pin_pad: mListPinOrPad){
			if(result != null){
				result.union(pin_pad.calculateBoundRectangle());
			}else{
				result = pin_pad.calculateBoundRectangle();
			}
		}
		return result;
	}
	
	private RectF calculateDraftLineOverallBoundRectangle(){
		// this function returns null of no pin or pad exists
		RectF result = null;
		for (PinOrPadOrDraftLine draftline : mListDraftLine) {
			if (result != null) {
				result.union(draftline.calculateBoundRectangle());
			} else {
				result = draftline.calculateBoundRectangle();
			}
		}
		return result;
	}
	
	public RectF calculateFootprintOverallBoundRectangle(){
		//this function returns null if there is no pin, pad or draftline exists
		RectF pin_rect, draftline_rect;
		pin_rect = calculatePinPadOverallBoundRectangle();
		draftline_rect = calculateDraftLineOverallBoundRectangle();
		if(pin_rect == null) return draftline_rect;
		if(draftline_rect == null) return pin_rect;
		//when both are not null
		pin_rect.union(draftline_rect);
		return pin_rect;
	}
	
	public void centerTheFootprint(){
		//Offset everything so that the center of the bound rectangle is (0,0)
		float ctX,ctY;
		RectF bound = calculateFootprintOverallBoundRectangle();
		if(bound == null) return;//need to do nothing
		ctX = bound.centerX();
		ctY = bound.centerY();
		if(ctX== 0 && ctY == 0) return;
		for(PinOrPadOrDraftLine pin_pad: mListPinOrPad){
			pin_pad.offset(-ctX, -ctY);
		}
		
		for (PinOrPadOrDraftLine draftline : mListDraftLine) {
			draftline.offset(-ctX, -ctY);
		}
		
		if(mMark!=null) mMark.offset(-ctX, -ctY);
		if(mTextLoc!=null) mTextLoc.offset(-ctX, -ctY);
	}
	
	public void offsetTheFootprint(float dx, float dy){
		if(dx== 0 && dy == 0) return;
		for(PinOrPadOrDraftLine pin_pad: mListPinOrPad){
			pin_pad.offset(dx, dy);
		}
		
		for (PinOrPadOrDraftLine draftline : mListDraftLine) {
			draftline.offset(dx, dy);
		}
	}
	/**
	 * String to flags. Convert the given string to flags according to gEDA document.
	 *
	 * @param str the str
	 * @return the int
	 */
	public static int stringToFlags(String str){
		//The string flags can be comma separated: "square,octal"
		//convert to lower characters
		String lstr = str.toLowerCase();
		int i=0,ind,flag=0;
		while((ind=lstr.indexOf(',',i))>=0){
			flag |= strFlagMap.get(lstr.substring(i, ind-1));
			i=ind+1;
		}
		return flag;
	}
	
	public static abstract class PinOrPadOrDraftLine{
		public static final int TYPE_PIN=1;
		public static final int TYPE_PAD=2;
		public static final int TYPE_LINE=3;
		public static final int TYPE_ARC=4;
		public static final int TYPE_MARK=5;
		
		public static final int SHAPE_ROUND=0;
		public static final int SHAPE_RECT=0x0100;
		public static final int SHAPE_OCT=0x0800;
		
		private int type;
		
		public abstract RectF calculateBoundRectangle();
		public abstract void offset(float dx, float dy);
		//public abstract Path toPath(int layer);
		
		public int getType() {
			return type;
		}

		public void setType(int type) {
			this.type = type;
		}
	}
	
	public static class Pin extends PinOrPadOrDraftLine{
		
		public float aX,aY,Thickness,Clearance,Mask,Drill;
		public String Name, Number;
		public int Flags;
		
		public Pin(float ax, float ay, float thickness, float clearance, float mask, float drill, String name, String number, int flags){
			aX=ax;aY=ay;Thickness=thickness;Clearance=clearance;Mask=mask;Drill=drill;
			Name=name;Number=number;
			Flags=flags;
			setType(TYPE_PIN);
		}
		
		@Override
		public RectF calculateBoundRectangle() {
			// Calculate the bound rectangle
			RectF rect = new RectF();
			float largest_Radius = findLargestDiameter()/2;
			rect.left = aX-largest_Radius;
			rect.right = aX+largest_Radius;
			rect.top = aY-largest_Radius;
			rect.bottom = aY+largest_Radius;
			return rect;
		}

		private float findLargestDiameter(){
			float largestDiameter = Thickness + Clearance;
			if(Mask> largestDiameter)largestDiameter = Mask;
			if(Drill > largestDiameter)largestDiameter = Drill;
			return largestDiameter;
		}

		@Override
		public void offset(float dx, float dy) {
			aX+=dx;
			aY+=dy;
		}
		
		public int getShape(){
			if((Flags & SHAPE_RECT)!=0) return SHAPE_RECT;
			if((Flags & SHAPE_OCT)!=0) return SHAPE_OCT;
			return SHAPE_ROUND;
		}
		
		private Path generateBound(float width_in_cmil, Path.Direction dir, float dpi){
			Path path = new Path();
			float aXdpi,aYdpi,raddpi;
			aXdpi=CentiMil.CentiMilToPixel(aX, dpi);
			aYdpi=CentiMil.CentiMilToPixel(aY, dpi);
			raddpi=CentiMil.CentiMilToPixel(width_in_cmil, dpi)/2;
			
			switch(getShape()){
			case SHAPE_ROUND:
				path.addCircle(aXdpi, aYdpi, raddpi, dir);
				return path;
			case SHAPE_RECT:
				
				break;
			case SHAPE_OCT:
				break;
			default:
				break;
			}
			return null;
		}
		
//		@Override
//		public Path toPath(int layer) {
//			switch(layer){
//			case ICFootprintRender.LAYER_DRAFT:
//				return null;
//			case ICFootprintRender.LAYER_MASK:
//				//decide shape
//				break;
//			case ICFootprintRender.LAYER_CLEARANCE:
//				break;
//			case ICFootprintRender.LAYER_DRILL:
//				break;
//			case ICFootprintRender.LAYER_COPPER:
//				break;
//			default:
//				return null;
//			}
//			return null;
//		}
	}
	
	public static class Pad extends PinOrPadOrDraftLine{
		
		public float aX1,aY1, aX2,aY2,Thickness,Clearance,Mask;
		public String Name, Number;
		public int Flags;
		
		public Pad(float ax1,float ay1, float ax2, float ay2, float thickness, float clearance, float mask, String name, String number, int flags){
			aX1=ax1;aY1=ay1;aX2=ax2;aY2=ay2;Thickness=thickness;Clearance=clearance;Mask=mask;
			Name=name;Number=number;
			Flags=flags;
			setType(TYPE_PAD);
		}
		
		@Override
		public RectF calculateBoundRectangle() {
			
			RectF rect = new RectF();
			float topY,btmY,leftX,rightX,largest_Radius;
			topY=(aY1<aY2)?aY1:aY2;
			btmY=(aY1>aY2)?aY1:aY2;
			leftX=(aX1<aX2)?aX1:aX2;
			rightX=(aX1>aX2)?aX1:aX2;
			largest_Radius = findLargestDiameter()/2;
			
			rect.left = leftX-largest_Radius;
			rect.right=rightX+largest_Radius;
			rect.top = topY-largest_Radius;
			rect.bottom = btmY+largest_Radius;
			return rect;
		}
		
		private float findLargestDiameter(){
			float largestDiameter = Thickness + Clearance;
			if(Mask> largestDiameter)largestDiameter = Mask;

			return largestDiameter;
		}
		@Override
		public void offset(float dx, float dy) {
			aX1+=dx;
			aY1+=dy;
			aX2+=dx;
			aY2+=dy;
		}
		
		public int getShape(){
			if((Flags & SHAPE_RECT)!=0) return SHAPE_RECT;
			if((Flags & SHAPE_OCT)!=0) return SHAPE_OCT;
			return SHAPE_ROUND;
		}
//		@Override
//		public Path toPath(int layer) {
//			switch(layer){
//			case ICFootprintRender.LAYER_DRAFT:
//				return null;
//			case ICFootprintRender.LAYER_MASK:
//				break;
//			case ICFootprintRender.LAYER_CLEARANCE:
//				break;
//			case ICFootprintRender.LAYER_DRILL:
//				return null;
//			case ICFootprintRender.LAYER_COPPER:
//				break;
//			default:
//				return null;
//			}
//			return null;
//		}
	}
	
//	public abstract class DraftLine{
//		public static final int TYPE_LINE=1;
//		public static final int TYPE_ARC=2;
//		
//		private int type;
//		public abstract RectF calculateBoundRectangle();
//		public int getType() {
//			return type;
//		}
//		public void setType(int type) {
//			this.type = type;
//		}
//	}
	
	public static class ElementLine extends PinOrPadOrDraftLine{
		public float aX1,aY1,aX2,aY2,Thickness;
		
		public ElementLine(float ax1, float ay1, float ax2, float ay2, float thickness){
			aX1=ax1;aY1=ay1;aX2=ax2;aY2=ay2;Thickness=thickness;
			setType(TYPE_LINE);
		}
		@Override
		public RectF calculateBoundRectangle() {
			RectF rect = new RectF();
			rect.top=(aY1<aY2)?aY1:aY2;
			rect.bottom=(aY1>aY2)?aY1:aY2;
			rect.left=(aX1<aX2)?aX1:aX2;
			rect.right=(aX1>aX2)?aX1:aX2;
			return rect;
		}
		@Override
		public void offset(float dx, float dy) {
			aX1+=dx;
			aY1+=dy;
			aX2+=dx;
			aY2+=dy;
		}
//		@Override
//		public Path toPath(int layer) {
//			switch(layer){
//			case ICFootprintRender.LAYER_DRAFT:
//				break;
//			case ICFootprintRender.LAYER_MASK:
//				return null;
//			case ICFootprintRender.LAYER_CLEARANCE:
//				return null;
//			case ICFootprintRender.LAYER_DRILL:
//				return null;
//			case ICFootprintRender.LAYER_COPPER:
//				return null;
//			default:
//				return null;
//			}
//			return null;
//		}
	}
	
	public static class ElementArc extends PinOrPadOrDraftLine{
		public float aX,aY,Width,Height,StartAngle,DeltaAngle,Thickness;
		
		public ElementArc(float ax,float ay, float wid, float hgt, float strt_ang, float del_ang, float thickness){
			aX=ax;aY=ay;Width=wid;Height=hgt;StartAngle=strt_ang;DeltaAngle=del_ang;Thickness=thickness;
			setType(TYPE_ARC);
		}
		private float calculateR(float degree){
			//degree to radius
			float rad = (float)(degree/180.0*PI);
			float wcos = Width*cos(rad);
			float hsin = Height*sin(rad);
			return Width*Height/sqrt(wcos*wcos+hsin*hsin);
		}
		
		@Override
		public RectF calculateBoundRectangle() {
			//a more accurate calculation of boundary
			float strt_ang, del_ang, end_ang;//the "regulated" start angle and delta angle. start angle must be between 0~360, delta angle must be positive
			del_ang = (DeltaAngle>=0)?DeltaAngle:-DeltaAngle;
			strt_ang = (DeltaAngle>=0)?StartAngle:StartAngle+DeltaAngle;//reverse the start angle of delta angle is negative
			strt_ang = strt_ang % 360.0f;//remainder operation, may get negative value.
			strt_ang = (strt_ang >= 0)?	strt_ang:strt_ang+360.0f;
			end_ang = strt_ang+del_ang;
			
			//now strt_ang 0~360 and delta angle is positive.
			//calculate strt_ang is in which quadrant
			//Note that geda define following quarant:
			//		3	|	2
			//0 deg-----|-----
			//		0	|	1
			int strt_quad, end_quad;
			strt_quad = (int)(strt_ang/90.0f);
			end_quad = (int)((strt_ang+del_ang)/90.0f);
			
			boolean included[]={false,false,false,false};//left,bottom,right,top
			for(int i=strt_quad+1;i<=end_quad;i++){
				included[i%4]=true;
			}
			
			float strtX,strtY,endX,endY,strtR,endR;
			strtR = calculateR(strt_ang);
			endR = calculateR(end_ang);
			//use sine/cosine to calculate start end XYs
			strtX = -strtR*cos(strt_ang);
			strtY = strtR*sin(strt_ang);
			endX = -endR*cos(end_ang);
			endY = endR*sin(end_ang);
			RectF rect = new RectF();
			if(!included[0])rect.left = min(strtX,endX);
			else rect.left = -Width;
			
			if(!included[1])rect.bottom = max(strtY,endY);
			else rect.bottom = Height;
			
			if(!included[2])rect.right = max(strtX,endX);
			else rect.right = Width;
			
			if(!included[3])rect.top = min(strtY,endY);
			else rect.top = -Height;
			
			//at last, offset to center
			rect.offset(aX, aY);
			
//			rect.top=aY-Height;
//			rect.bottom=aY+Height;
//			rect.left=aX-Width;
//			rect.right=aX+Width;
			return rect;
		}
		
		@Override
		public void offset(float dx, float dy) {
			aX+=dx;
			aY+=dy;
		}
//		@Override
//		public Path toPath(int layer) {
//			switch(layer){
//			case ICFootprintRender.LAYER_DRAFT:
//				break;
//			case ICFootprintRender.LAYER_MASK:
//				return null;
//			case ICFootprintRender.LAYER_CLEARANCE:
//				return null;
//			case ICFootprintRender.LAYER_DRILL:
//				return null;
//			case ICFootprintRender.LAYER_COPPER:
//				return null;
//			default:
//				return null;
//			}
//			return null;
//		}
	}
	
	public static class Mark extends PinOrPadOrDraftLine{
		public float aX,aY;
		
		public float getaX() {
			return aX;
		}

		public float getaY() {
			return aY;
		}

		public Mark(float absX,float absY) {
			aX=absX;aY=absY;
			setType(TYPE_MARK);
		}
		
		public Mark(int absX,int absY){
			aX=(float)absX;aY=(float)absY;
			setType(TYPE_MARK);
		}

		public void offset(float dx, float dy) {
			aX+=dx;
			aY+=dy;
		}

		@Override
		public RectF calculateBoundRectangle() {
			
			return new RectF(aX,aY,aX,aY);
		}	
	}
	
	public static final class CentiMil{
		//a unified coordinate uses 1/100 mil as base unit.
		//and includes conversion functions
		
		public static final int UNIT_CMIL	= 0;
		public static final int UNIT_UMIL	= 1;
		public static final int UNIT_MIL	= 2;
		public static final int UNIT_INCH	= 3;
		public static final int UNIT_NM	= 4;
		public static final int UNIT_UM	= 5;
		public static final int UNIT_MM	= 6;
		public static final int UNIT_M		= 7;
		public static final int UNIT_KM	= 8;
		public static final int UNIT_PIXEL = 9;
		
		static final float INCH_TO_M = 0.0254f;
		
		private static float HowMuchCmilPerType(int type){
			switch(type){
			case UNIT_CMIL:
				return 1.0f;
			case UNIT_UMIL:
				return 1/10000;
			case UNIT_MIL:
				return 100;
			case UNIT_INCH:
				return 100000;
			case UNIT_NM:
				return 1/10000/INCH_TO_M;
			case UNIT_UM:
				return 0.1f/INCH_TO_M;
			case UNIT_MM:
				return 100/INCH_TO_M;
			case UNIT_M:
				return 100000/INCH_TO_M;
			case UNIT_KM:
				return 100000000/INCH_TO_M;
			default:
				return 1.0f;
			}
		}
		
//		private float mValue;//this is in cmil (1/100mil)
//
//		public CentiMil(float d, int type) {
//			mValue = d*HowMuchCmilPerType(type);
//		}
//		
//		public CentiMil(int v, int type){
//			mValue = (float)v*HowMuchCmilPerType(type);
//		}
		
		public static final float toCentiMil(float value, int unit_type){
			return value*HowMuchCmilPerType(unit_type);
		}
		public static final float toCentiMil(int value, int unit_type){
			return (float)value*HowMuchCmilPerType(unit_type);
		}
		
		public static final float CentiMilToPixel(float CentiMil, float dpi){
			return CentiMil/HowMuchCmilPerType(UNIT_INCH)*dpi;
		}
		
		public static final float PixelToCentiMil(float pixel, float dpi){
			return pixel/dpi*HowMuchCmilPerType(UNIT_INCH);
		}
//		public float getValue() {
//			return mValue;
//		}
		
		
	}
	
	public static final class ICText{
		public float aX;
		public float aY;
		public float dir;
		public float scale;
		public int flags;
		public String description;
		public String name;
		public String value;
	}
}