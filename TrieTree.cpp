#include "stdafx.h"
#include "MainWindow.h"
#include "TrieTree.h"

extern  CMainWindow *pMaindlg;

map<string, LIBMAP> m_LibMap;	//用于显示结果

TrieTreeNode::TrieTreeNode() {
	ChildNodes = new TrieTreeNode*[256];
	for (int i = 0;i < 256;i++) {
		ChildNodes[i] = NULL;
	}
	EsigText = NULL;
	FuncName = NULL;
	IsMatched = false;
}



BOOL TrieTree::LoadSig(const char* lpMapPath) {
	HANDLE hFile = CreateFileA(lpMapPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD	dwHitSize = 0;
	DWORD	dwSize = GetFileSize(hFile, &dwHitSize);
	DWORD	dwReadSize = 0;
	char* pMap = (char*)malloc(dwSize);

	ReadFile(hFile, pMap, dwSize, &dwReadSize, NULL);	//读取文本至内存

	string Sig = pMap;

	string Config = GetMidString(Sig, "******Config******\r\n", "******Config_End******", 0);

	if (Config.find("IsAligned=true") != -1) {
		IsAligned = true;
	}
	else
	{
		IsAligned = false;
	}

	string SubFunc = GetMidString(Sig, "*****SubFunc*****\r\n", "*****SubFunc_End*****", 0);

	int pos = SubFunc.find("\r\n");     //子函数

	while (pos != -1) {
		string temp = SubFunc.substr(0, pos);  //单个子函数
		int tempos = temp.find(':');
		if (tempos == -1) {
			break;
		}
		m_subFunc[temp.substr(0, tempos)] = temp.substr(tempos + 1);
		SubFunc = SubFunc.substr(pos + 2);
		pos = SubFunc.find("\r\n");
	}


	string Func = GetMidString(Sig, "***Func***\r\n", "***Func_End***", 0);

	pos = Func.find("\r\n");

	while (pos != -1) {
		string temp = Func.substr(0, pos);    //单个子函数
		int tempos = temp.find(':');
		if (tempos == -1) {
			break;
		}
		if (!Insert(temp.substr(tempos + 1), temp.substr(0, tempos))) {
			MessageBoxA(NULL, "插入函数失败", "12", 0);
		}
		Func = Func.substr(pos + 2);
		pos = Func.find("\r\n");
	}


	if (pMap) {
		free(pMap);
	}

	CloseHandle(hFile);
	return TRUE;
}

char* TrieTree::MatchSig(UCHAR* CodeSrc) {
	TrieTreeNode* p = root;		//当前指针指向root
	return Match(p, (UCHAR*)CodeSrc);
}

void TrieTree::ScanSig(UCHAR* CodeSrc, ULONG SrcLen, string& LibName) {

	TrieTreeNode* p = root;		//当前指针指向root

	if (IsAligned) {
		for (ULONG offset = 0;offset < SrcLen-16;offset = offset + 16) {		//有一个问题没解决，就是扫描到边界的处理,暂时-16处理
			char* FuncName = Match(p, (UCHAR*)CodeSrc + offset);
			if (FuncName) {
				DWORD dwAddr = pEAnalysisEngine->V2O((DWORD)CodeSrc + offset, 0);
				m_LibMap[LibName].Command_addr.push_back(dwAddr);
				m_LibMap[LibName].Command_name.push_back(FuncName);
				Insertname(dwAddr, NM_LABEL, FuncName);
			}
		}
	}
	else {
		for (ULONG offset = 0;offset < SrcLen - 16;offset++) {			
			char* FuncName = Match(p, (UCHAR*)CodeSrc + offset);
			if (FuncName) {
				DWORD dwAddr = pEAnalysisEngine->V2O((DWORD)CodeSrc + offset, 0);
				m_LibMap[LibName].Command_addr.push_back(dwAddr);
				m_LibMap[LibName].Command_name.push_back(FuncName);
				Insertname(dwAddr, NM_LABEL, FuncName);
			}
		}
	}
	return;
}

BOOL TrieTree::Insert(string& FuncTxt, const string& FuncName) {		//参数一为函数的文本形式,参数二为函数的名称
	TrieTreeNode *p = root;		//将当前节点指针指向ROOT节点

	string BasicTxt;
	string SpecialTxt;

	for (UINT n = 0;n < FuncTxt.length();n++) {
		switch (FuncTxt[n])
		{
		case '-':	//Check 1次
			if (FuncTxt[n + 1] == '-' && FuncTxt[n + 2] == '>')
			{
				BasicTxt = "E9";
				p = AddNode(p, BasicTxt);
				p = AddSpecialNode(p, TYPE_LONGJMP, "");
				n = n + 2;
				continue;		//此continue属于外部循环
			}
			return false;
		case '<':
			if (FuncTxt[n + 1] == '[') {						//CALLAPI
				int post = FuncTxt.find("]>", n);
				if (post == -1) {
					return false;
				}
				BasicTxt = "FF";
				p = AddNode(p, BasicTxt);
				BasicTxt = "15";
				p = AddNode(p, BasicTxt);
				SpecialTxt = FuncTxt.substr(n + 2, post - n - 2);   //得到文本中的IAT函数
				p = AddSpecialNode(p, TYPE_CALLAPI, SpecialTxt);
				n = post + 1;
				continue;
			}
			else {											//普通的函数CALL
				int post = FuncTxt.find('>', n);
				if (post == -1) {
					return false;
				}
				SpecialTxt = FuncTxt.substr(n + 1, post - n - 1);
				BasicTxt = "E8";
				p = AddNode(p, BasicTxt);
				p = AddSpecialNode(p, TYPE_CALL, SpecialTxt);
				n = post;
				continue;
			}
		case '[':
			if (FuncTxt[n + 1] == ']' && FuncTxt[n + 2] == '>') {
				//To Do
			}
			else
			{
				int post = FuncTxt.find(']', n);
				if (post == -1) {
					return false;
				}
				BasicTxt = "FF";
				p = AddNode(p, BasicTxt);
				BasicTxt = "25";
				p = AddNode(p, BasicTxt);
				SpecialTxt = FuncTxt.substr(n + 1, post - n - 1);
				p = AddSpecialNode(p, TYPE_JMPAPI, SpecialTxt);
				n = post;
				continue;
			}
		case '?':
			if (FuncTxt[n + 1] == '?') {
				p = AddSpecialNode(p, TYPE_ALLPASS, FuncTxt.substr(n, 2));	//全通配符
				n = n + 1;
				continue;
			}
			else
			{
				p = AddSpecialNode(p, TYPE_LEFTPASS, FuncTxt.substr(n, 2));	//左通配符
				n = n + 1;
				continue;
			}
		case '!':	//验证1次
		{
			int post = FuncTxt.find('!', n + 1);	//从!的下一个字符开始寻找!
			if (post == -1) {
				return false;
			}
			SpecialTxt = FuncTxt.substr(n + 1, post - n - 1);
			p = AddSpecialNode(p, TYPE_CONSTANT, SpecialTxt);
			n = post;	//将当前指针指向右边的!号
			continue;
		}
		default:
			if (FuncTxt[n + 1] == '?') {
				p = AddSpecialNode(p, TYPE_RIGHTPASS, FuncTxt.substr(n, 2));	//右通配符
				n = n + 1;
				continue;
			}
			else {
				BasicTxt = FuncTxt.substr(n, 2);
				p = AddNode(p, BasicTxt);
				n = n + 1;
				continue;
			}
		}
	}

	if (p->FuncName) {		//确保函数名称唯一性！！！
		MessageBoxA(NULL, p->FuncName, "发现相同函数", 0);
		return false;
	}
	
	p->FuncName = new char[FuncName.length() + 1];strcpy_s(p->FuncName, FuncName.length() + 1, FuncName.c_str());
	return true;
}

TrieTreeNode* TrieTree::AddNode(TrieTreeNode* p, string& Txt) {
	UCHAR index = 0;
	HexToBin(Txt, &index);
	if (p->ChildNodes[index]) {
		return p->ChildNodes[index];
	}

	TrieTreeNode* NewNode = new TrieTreeNode(); //如果所有的节点中都没有,则创建一个新节点
	p->ChildNodes[index] = NewNode;      //当前节点加入新子节点

	NewNode->EsigText = new char[Txt.length() + 1];strcpy_s(NewNode->EsigText, Txt.length() + 1, Txt.c_str());//赋值EsigTxt
	NewNode->SpecialType = TYPE_NORMAL;
	return NewNode;
}

TrieTreeNode* TrieTree::AddSpecialNode(TrieTreeNode* p, UINT type, string Txt) {		//特殊节点数组
	for (int i = 0;i < p->SpecialNodes.size();i++) {		//遍历当前子节点
		if (p->SpecialNodes[i]->SpecialType == type && Txt.compare(p->SpecialNodes[i]->EsigText) == 0) {
			return p->SpecialNodes[i];
		}
	}

	TrieTreeNode* NewNode = new TrieTreeNode(); //如果所有的节点中都没有,则创建一个新节点
	p->SpecialNodes.push_back(NewNode);      //当前节点加入新子节点
	NewNode->EsigText = new char[Txt.length() + 1];strcpy_s(NewNode->EsigText, Txt.length() + 1, Txt.c_str());//赋值EsigTxt
	NewNode->SpecialType = type;
	return NewNode;
}

BOOL TrieTree::CmpCode(UCHAR* FuncSrc, string& FuncTxt) {
	USES_CONVERSION;
	UCHAR* pSrc = FuncSrc;  //初始化函数代码指针

	for (UINT n = 0;n < FuncTxt.length();n++) {
		switch (FuncTxt[n])
		{
		case '-':	//Check 1次
			if (FuncTxt[n + 1] == '-' && FuncTxt[n + 2] == '>') {		//长跳转
				if (*pSrc != 0xE9) {
					return false;
				}
				ULONG JmpSrc = (ULONG)pSrc + *(DWORD*)(pSrc + 1) + 5;	//先取得jmp的虚拟地址

				INT BaseIndex = pEAnalysisEngine->FindVirutalSection((ULONG)pSrc);	//以当前区段为参考系

				ULONG oaddr = pEAnalysisEngine->V2O(JmpSrc, BaseIndex);	//转换为实际代码中的地址

				INT index = pEAnalysisEngine->FindOriginSection(oaddr);	//判断跳转是否合理
				if (index == -1) {
					index = pEAnalysisEngine->AddSection(oaddr);
					if (index == -1) {
						return false;
					}
				}
				pSrc = (UCHAR*)pEAnalysisEngine->O2V(oaddr, index);
				n = n + 2;
				continue;
			}
			break;
		case '<':		//Check 2次
			if (FuncTxt[n + 1] == '[') {						//CALLAPI
				if (*pSrc != 0xFF || *(pSrc + 1) != 0x15) {
					return false;
				}
				int post = FuncTxt.find("]>", n);
				if (post == -1) {
					return false;
				}
				string IATEAT = FuncTxt.substr(n + 2, post - n - 2);   //得到文本中的IAT函数

				string IATCom;
				string EATCom;
				char buffer[256] = { 0 };
				int EATpos = IATEAT.find("||");
				if (EATpos != -1) {            //存在自定义EAT
					IATCom = IATEAT.substr(0, EATpos);
					EATCom = IATEAT.substr(EATpos + 2);
				}
				else
				{
					IATCom = IATEAT;
					EATCom = IATEAT.substr(IATEAT.find('.') + 1);
				}

				ULONG oaddr = *(ULONG*)(pSrc + 2);		//得到IAT函数的真实地址

				if (Findname(oaddr, NM_IMPORT, buffer) != 0 && stricmp(IATCom.c_str(),buffer) == 0) {  //首先IAT匹配
					pSrc = pSrc + 6;
					n = post + 1;
					continue;
				}
				
				INT index = pEAnalysisEngine->FindOriginSection(oaddr);	//寻找地址所在index
				if (index == -1) {
					index = pEAnalysisEngine->AddSection(oaddr);
					if (index == -1) {
						return false;
					}
				}

				if (Findname(*(ULONG*)pEAnalysisEngine->O2V(oaddr, index), NM_EXPORT, buffer) != 0 && stricmp(EATCom.c_str(),buffer) == 0) {      //EAT匹配
					pSrc = pSrc + 6;
					n = post + 1;
					continue;
				}
				return false;
			}
			else					  //NORMALCALL Check 1次
			{
				if (*pSrc != 0xE8) {
					return false;
				}
				int post = FuncTxt.find('>', n);
				if (post == -1) {
					return false;
				}
				string SubFunc = FuncTxt.substr(n + 1, post - n - 1);   //子函数名称

				ULONG CallSrc = (ULONG)pSrc + *(DWORD*)(pSrc + 1) + 5;	//得到虚拟地址

				INT BaseIndex = pEAnalysisEngine->FindVirutalSection((ULONG)pSrc);	//以当前区段为参考系

				ULONG oaddr = pEAnalysisEngine->V2O(CallSrc, BaseIndex);	//转换为实际代码中的地址

				if (m_RFunc[oaddr] == SubFunc) {       //为什么不先检查函数地址合法性?因为绝大部分函数都是合法的
					pSrc = pSrc + 5;
					n = post;
					continue;
				}

				INT index = pEAnalysisEngine->FindOriginSection(oaddr);
				if (index == -1) {		//CALL到其它区段了...
					index = pEAnalysisEngine->AddSection(oaddr);
					if (index == -1) {
						return false;
					}
				}

				if (CmpCode((UCHAR*)pEAnalysisEngine->O2V(oaddr, index), m_subFunc[SubFunc])) {
					pSrc = pSrc + 5;
					n = post;
					m_RFunc[oaddr] = SubFunc;
					continue;
				}
				return false;
			}
			break;
		case '[':		// Check 1次
			if (*pSrc != 0xFF || *(pSrc + 1) != 0x25) {
				return false;
			}
			if (FuncTxt[n + 1] == ']' && FuncTxt[n + 2] == '>') {		//FF25跳转
																		//To Do
			}
			else {								//FF25 IATEAT
				int post = FuncTxt.find(']', n);
				if (post == -1) {
					return false;
				}
				string IATEAT = FuncTxt.substr(n + 1, post - n - 1);

				string IATCom;
				string EATCom;
				char buffer[256] = { 0 };
				int EATpos = IATEAT.find("||");
				if (EATpos != -1) {            //存在自定义EAT
					IATCom = IATEAT.substr(0, EATpos);
					EATCom = IATEAT.substr(EATpos + 2);
				}
				else
				{
					IATCom = IATEAT;
					EATCom = IATEAT.substr(IATEAT.find('.') + 1);
				}

				ULONG addr = *(ULONG*)(pSrc + 2);
				if (Findname(addr, NM_IMPORT, buffer) != 0 && stricmp(IATCom.c_str(),buffer) == 0) {  //首先IAT匹配
					pSrc = pSrc + 6;
					n = post;
					continue;
				}

				int index = pEAnalysisEngine->FindOriginSection(addr);
				if (index == -1) {
					index = pEAnalysisEngine->AddSection(addr);
					if (index == -1) {
						return false;
					}
				}

				if (Findname(*(ULONG*)pEAnalysisEngine->O2V(addr, index), NM_EXPORT, buffer) != 0 && stricmp(EATCom.c_str(),buffer) == 0) {      //EAT匹配
					pSrc = pSrc + 6;
					n = post;
					continue;
				}
				return false;
			}
			break;
		case '!':
		{
			int post = FuncTxt.find('!', n + 1);
			if (post == -1) {
				return false;
			}

			string Conname = FuncTxt.substr(n + 1, post - n - 1);
	
			ULONG ConSrc = *(ULONG*)pSrc;
			INT ConIndex = pEAnalysisEngine->FindOriginSection(ConSrc);	//寻找地址所在index
			if (ConIndex == -1) {		//在区段外则添加区段
				ConIndex = pEAnalysisEngine->AddSection(ConSrc);
				if (ConIndex == -1) {
					pMaindlg->outputInfo("Type_Constant申请内存失败");
					return false;
				}
			}
			if (m_RFunc[ConSrc] == Conname || CmpCode((UCHAR*)pEAnalysisEngine->O2V(ConSrc, ConIndex), m_subFunc[Conname])) {
				pSrc = pSrc + 4;
				n = post;
				continue;
			}
			return false;
		}
		case '?':
			if (FuncTxt[n + 1] == '?') {	//全通配符
				n++;
				pSrc++;
				continue;
			}
			else {							//左通配符
											//To Do
			}
		default:
			if (FuncTxt[n + 1] == '?') {	//右通配符
											//To Do
			}
			else {							//普通的代码
				UCHAR ByteCode = 0;
				HexToBin(FuncTxt.substr(n, 2), &ByteCode);
				if (*pSrc == ByteCode) {
					n++;
					pSrc++;
					continue;
				}
				return false;
			}
		}
	}
	return true;
}

BOOL TrieTree::CheckNode(TrieTreeNode* p,UCHAR** FuncSrc) {
	switch (p->SpecialType)
	{
	case TYPE_NORMAL:
	{
		return true;
	}
	case TYPE_LONGJMP:
	{
		ULONG JmpSrc = (ULONG)*FuncSrc - 1 + *(DWORD*)(*FuncSrc)+5;	//先取得jmp的虚拟地址
		INT BaseIndex = pEAnalysisEngine->FindVirutalSection((ULONG)*FuncSrc);	//以当前区段为参考系
		ULONG oaddr = pEAnalysisEngine->V2O(JmpSrc, BaseIndex);	//转换为实际代码中的地址
		INT index = pEAnalysisEngine->FindOriginSection(oaddr);	//判断跳转是否合理
		if (index == -1) {
			index = pEAnalysisEngine->AddSection(oaddr);
			if (index == -1) {
				return false;
			}
		}
		*FuncSrc = (UCHAR*)pEAnalysisEngine->O2V(oaddr, index);
		return true;
	}
	case TYPE_CALL:
	{
		ULONG CallSrc = (ULONG)*FuncSrc - 1 + *(DWORD*)(*FuncSrc) + 5;	//得到虚拟地址
		INT BaseIndex = pEAnalysisEngine->FindVirutalSection((ULONG)*FuncSrc);	//以当前区段为参考系
		ULONG oaddr = pEAnalysisEngine->V2O(CallSrc, BaseIndex);	//转换为实际代码中的地址
		if (m_RFunc[oaddr] == p->EsigText) {		//此函数已经匹配过一次
			*FuncSrc = *FuncSrc + 4;
			return true;
		}
		INT index = pEAnalysisEngine->FindOriginSection(oaddr);
		if (index == -1) {		//CALL到其它区段了...
			index = pEAnalysisEngine->AddSection(oaddr);
			if (index == -1) {
				return false;
			}
		}
		if (!CmpCode((UCHAR*)pEAnalysisEngine->O2V(oaddr, index), m_subFunc[p->EsigText])) {
			return false;
		}
		*FuncSrc = *FuncSrc + 4;
		return true;
	}
	case TYPE_JMPAPI:
	{
		string IATEAT = p->EsigText;
		string IATCom;
		string EATCom;
		char buffer[256] = { 0 };

		int EATpos = IATEAT.find("||");
		if (EATpos != -1) {            //存在自定义EAT
			IATCom = IATEAT.substr(0, EATpos);
			EATCom = IATEAT.substr(EATpos + 2);
		}
		else
		{
			IATCom = IATEAT;
			EATCom = IATEAT.substr(IATEAT.find('.') + 1);
		}

		ULONG addr = *(ULONG*)*FuncSrc;
		if (Findname(addr, NM_IMPORT, buffer) != 0 && stricmp(IATCom.c_str(), buffer) == 0) {
			*FuncSrc = *FuncSrc + 4;
			return true;
		}
		int index = pEAnalysisEngine->FindOriginSection(addr);
		if (index == -1) {
			index = pEAnalysisEngine->AddSection(addr);
			if (index == -1) {
				return false;
			}
		}
		if (Findname(*(ULONG*)pEAnalysisEngine->O2V(addr, index), NM_EXPORT, buffer) != 0 && stricmp(EATCom.c_str(), buffer) == 0) {      //EAT匹配
			*FuncSrc = *FuncSrc + 4;
			return true;
		}
		return false;
	}
	case TYPE_CALLAPI:
	{
		string IATEAT = p->EsigText;

		string IATCom;
		string EATCom;
		char buffer[256] = { 0 };

		int EATpos = IATEAT.find("||");
		if (EATpos != -1) {					//存在自定义EAT
			IATCom = IATEAT.substr(0, EATpos);
			EATCom = IATEAT.substr(EATpos + 2);
		}
		else
		{
			IATCom = IATEAT;
			EATCom = IATEAT.substr(IATEAT.find('.') + 1);
		}

		ULONG oaddr = *(ULONG*)(*FuncSrc);			//得到IAT函数的真实地址

		if (Findname(oaddr, NM_IMPORT, buffer) != 0 && stricmp(IATCom.c_str(), buffer) == 0) {		//首先IAT匹配
			*FuncSrc = *FuncSrc + 4;
			return true;
		}

		INT index = pEAnalysisEngine->FindOriginSection(oaddr);	//寻找地址所在index
		if (index == -1) {
			index = pEAnalysisEngine->AddSection(oaddr);
			if (index == -1) {
				return false;
			}
		}

		if (Findname(*(ULONG*)pEAnalysisEngine->O2V(oaddr, index), NM_EXPORT, buffer) != 0 && stricmp(EATCom.c_str(), buffer) == 0) {      //EAT匹配
			*FuncSrc = *FuncSrc + 4;
			return true;
		}
		return false;
	}
	case TYPE_CONSTANT:
	{
		ULONG ConSrc = *(ULONG*)*FuncSrc;	//取得实际地址
		INT ConIndex = pEAnalysisEngine->FindOriginSection(ConSrc);	//寻找地址所在index
		if (ConIndex == -1) {		//在区段外则添加区段
			ConIndex = pEAnalysisEngine->AddSection(ConSrc);
			if (ConIndex == -1) {
				return false;
			}
		}
		if (m_RFunc[ConSrc] == p->EsigText || CmpCode((UCHAR*)pEAnalysisEngine->O2V(ConSrc, ConIndex), m_subFunc[p->EsigText])) {
			*FuncSrc = *FuncSrc + 4;
			return true;
		}
		return false;
	}
	case TYPE_ALLPASS:
	{
		*FuncSrc = *FuncSrc + 1;
		return true;
	}
	default:
		break;
	}

	//To do 增加半通配符

	//if ((p->SpecialNodes[i]->EsigText[0] == '?' || HByteToBin(p->SpecialNodes[i]->EsigText[0])==*(FuncSrc)>>4) &&(p->SpecialNodes[i]->EsigText[1] == '?'|| HByteToBin(p->SpecialNodes[i]->EsigText[1]) == *(FuncSrc)& 0x0F)) {		//第一个是通配符
	//	return Match(p->SpecialNodes[i], FuncSrc + 1);		//继续匹配下一层
	//}
	return false;
}

char* TrieTree::Match(TrieTreeNode* p,UCHAR* FuncSrc) {	

	stack<TrieTreeNode*> StackNode;	//节点
	stack<UCHAR*> StackFuncSrc;		//节点地址

	StackNode.push(p);
	StackFuncSrc.push(FuncSrc);
	//进入循环初始条件

	while (!StackNode.empty()) {
		p = StackNode.top();
		StackNode.pop();
		FuncSrc = StackFuncSrc.top();
		StackFuncSrc.pop();
		//取回堆栈顶端节点
		if (p->FuncName) {
			return p->FuncName;
		}
		if (!CheckNode(p, &FuncSrc)) {
			continue;
		}
		//判断结果
		for (UINT i = 0;i < p->SpecialNodes.size();i++) {
			StackNode.push(p->SpecialNodes[i]);
			StackFuncSrc.push(FuncSrc);
		}
		if (p->ChildNodes[*FuncSrc]) {
			StackNode.push(p->ChildNodes[*FuncSrc]);
			StackFuncSrc.push(FuncSrc + 1);
		}
	}

	return NULL;
}

void TrieTree::Destroy(TrieTreeNode* p) {
	if (!p) {
		return;
	}

	for (int i = 0;i < p->SpecialNodes.size();i++) {
		Destroy(p->SpecialNodes[i]);
	}

	for (int i = 0;i < 256;i++) {
		Destroy(p->ChildNodes[i]);
	}

	if (p->EsigText) {
		delete[] p->EsigText;
		p->EsigText = NULL;
	}

	if (p->FuncName) {
		delete[] p->FuncName;
		p->FuncName = NULL;
	}

	delete p;
	p = NULL;
}

TrieTree::TrieTree()
{
	root = new TrieTreeNode();
	root->SpecialType = TYPE_NORMAL;
}